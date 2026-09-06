#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libssh2.h>
#include <mbedtls/pk.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <psa/crypto.h>
#include <mbedtls/platform_util.h>
#include <fcntl.h>
#include <errno.h>
#include <libssh2_sftp.h>

int send_wol_native(const char *broadcast_ip, const char *mac_str) {
    unsigned char packet[102];
    unsigned char mac[6];

    if (sscanf(mac_str, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        return -1;
    }

    memset(packet, 0xFF, 6);
    for (int i = 1; i <= 16; i++) {
        memcpy(packet + (i * 6), mac, 6);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt));

    struct sockaddr_in adder;
    memset(&adder, 0, sizeof(adder));
    adder.sin_family = AF_INET;
    adder.sin_port = htons(9);
    inet_pton(AF_INET, broadcast_ip, &adder.sin_addr);

    ssize_t sent = sendto(
            sock,
            packet,
            sizeof(packet),
            0,
            (struct sockaddr *) &adder,
            sizeof(adder)
    );
    close(sock);
    return (sent == sizeof(packet)) ? 0 : -1;
}

int generate_ed25519_keypair(const char *key_path) {
    if (access(key_path, F_OK) == 0) return 0;

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) return -1;

    char temp_path[PATH_MAX];
    int n = snprintf(temp_path, sizeof(temp_path), "%s.tmp.%d", key_path, getpid());
    if (n < 0 || (size_t)n >= sizeof(temp_path)) return -1;

    int fd = open(temp_path, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        return -1;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes,
                            PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_EXPORT);
    psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
    psa_set_key_type(&attributes,
                     PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
    psa_set_key_bits(&attributes, 255);

    psa_key_id_t key_id = 0;
    status = psa_generate_key(&attributes, &key_id);
    if (status != PSA_SUCCESS) {
        psa_reset_key_attributes(&attributes);
        close(fd);
        unlink(temp_path);
        return -1;
    }

    unsigned char private_key[32];
    size_t private_len = 0;
    status = psa_export_key(
            key_id,
            private_key,
            sizeof(private_key),
            &private_len
    );

    psa_destroy_key(key_id);
    psa_reset_key_attributes(&attributes);

    if (status != PSA_SUCCESS || private_len != sizeof(private_key)) {
        mbedtls_platform_zeroize(private_key, sizeof(private_key));
        close(fd);
        unlink(temp_path);
        return -1;
    }

    ssize_t written = write(fd, private_key, private_len);
    mbedtls_platform_zeroize(private_key, sizeof(private_key));

    if (written != (ssize_t)private_len) {
        close(fd);
        unlink(temp_path);
        return -1;
    }

    if (fsync(fd) != 0) {
        close(fd);
        unlink(temp_path);
        return -1;
    }
    close(fd);

    if(rename(temp_path, key_path) != 0) {
        unlink(temp_path);
        return -1;
    }

    return 0;
}

static inline int is_space_or_tab(char c) {
    return c == ' ' || c == '\t';
}

static int extract_key_fields(const char *line, size_t line_len,
                              const char **type_start, size_t *type_len,
                              const char **data_start, size_t *data_len) {
    size_t pos = 0;

    while (pos < line_len && is_space_or_tab(line[pos])) pos++;
    *type_start = line + pos;
    while (pos < line_len && !is_space_or_tab(line[pos])) pos++;
    *type_len = (line + pos) - *type_start;
    if (*type_len == 0) return -1;

    while (pos < line_len && is_space_or_tab(line[pos])) pos++;
    *data_start = line + pos;
    while (pos < line_len && !is_space_or_tab(line[pos])) pos++;
    *data_len = (line + pos) - *data_start;
    if (*data_len == 0) return -1;

    return 0;
}

static int keys_match(const char *line_a, size_t len_a,
                      const char *line_b, size_t len_b) {
    const char *a_type, *a_data, *b_type, *b_data;
    size_t a_type_len, a_data_len, b_type_len, b_data_len;

    if (extract_key_fields(
            line_a, len_a, &a_type,
            &a_type_len,&a_data, &a_data_len) != 0)
        return 0;
    if (extract_key_fields(
            line_b, len_b, &b_type,
            &b_type_len, &b_data, &b_data_len) != 0)
        return 0;

    return a_type_len == b_type_len &&
           a_data_len == b_data_len &&
           memcmp(a_type, b_type, a_type_len) == 0 &&
           memcmp(a_data, b_data, a_data_len) == 0;
}

int remote_key_already_present(
        LIBSSH2_SFTP *sftp_session,
        const char *pubkey_str,
        long pubkey_len) {
    LIBSSH2_SFTP_HANDLE *handle = libssh2_sftp_open(
            sftp_session,
            ".ssh/authorized_keys",
            LIBSSH2_FXF_READ, 0);
    if (!handle) {
        return 0;
    }

    size_t key_cmp_len = (size_t) pubkey_len;
    if (key_cmp_len > 0 && pubkey_str[key_cmp_len - 1] == '\n') key_cmp_len--;
    if (key_cmp_len > 0 && pubkey_str[key_cmp_len - 1] == '\r') key_cmp_len--;

    char buf[8192];
    char *remote_contents = NULL;
    size_t remote_len = 0;
    for (;;) {
        ssize_t n = libssh2_sftp_read(handle, buf, sizeof(buf));
        if (n < 0) {
            free(remote_contents);
            libssh2_sftp_close(handle);
            return -1;
        }
        if (n == 0) break;

        char *grown = realloc(remote_contents, remote_len + (size_t)n);
        if (!grown) {
            free(remote_contents);
            libssh2_sftp_close(handle);
            return -1;
        }
        remote_contents = grown;
        memcpy(remote_contents + remote_len, buf, (size_t)n);
        remote_len += (size_t)n;
    }
    libssh2_sftp_close(handle);

    int found = 0;
    if (remote_contents && key_cmp_len > 0) {
        size_t pos = 0;
        while (pos < remote_len) {
            size_t line_start = pos;
            while (pos < remote_len && remote_contents[pos] != '\n') pos++;

            size_t line_end = pos;
            if (line_end > line_start && remote_contents[line_end - 1] == '\r') {
                line_end--;
            }

            size_t line_len = line_end - line_start;
            if (keys_match(remote_contents + line_start, line_len, pubkey_str, key_cmp_len)) {
                found = 1;
                break;
            }
            if (pos < remote_len) pos++;
        }
    }

    free(remote_contents);
    return found;
}

int setup_remote_public_key(
        const char *ip,
        const char *user,
        const char *password,
        const char *pubkey_path) {
    if (libssh2_init(0)) return -1;

    FILE *f = fopen(pubkey_path, "rb");
    if (!f) return -1;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return -1;
    }
    long pubkey_len = ftell(f);
    if (pubkey_len <= 0) { fclose(f); return  -1; }
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    char *pubkey_str = malloc((size_t) pubkey_len + 2);
    if (!pubkey_str) {
        fclose(f);
        return -1;
    }

    size_t nread = fread(pubkey_str, 1, (size_t) pubkey_len, f);
    fclose(f);
    if (nread != (size_t)pubkey_len) {
        free(pubkey_str);
        return -1;
    }
    if (pubkey_str[pubkey_len - 1] != '\n') {
        pubkey_str[pubkey_len] = '\n';
        pubkey_len++;
    }
    pubkey_str[pubkey_len] = '\0';

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        free(pubkey_str);
        return -1;
    }

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(22);
    if (inet_pton(AF_INET, ip, &sin.sin_addr) != 1) {
        free(pubkey_str);
        close(sock);
        return -1;
    }

    if (connect(sock, (struct sockaddr *)(&sin), sizeof(sin)) != 0) {
        free(pubkey_str);
        close(sock);
        return -1;
    }

    LIBSSH2_SESSION *session = libssh2_session_init();
    if (!session) {
        free(pubkey_str);
        close(sock);
        return -1;
    }

    libssh2_session_set_blocking(session, 1);

    if (libssh2_session_handshake(session, sock) != 0) {
        libssh2_session_free(session);
        free(pubkey_str);
        close(sock);
        return -1;
    }

    if (libssh2_userauth_password(session, user, password) != 0) {
        libssh2_session_disconnect(session, "Auth Failed");
        libssh2_session_free(session);
        free(pubkey_str);
        close(sock);
        return -1;
    }

    int rc = -1;
    LIBSSH2_SFTP *sftp_session = libssh2_sftp_init(session);
    if (!sftp_session) goto cleanup_session;

    int mkdir_rc = libssh2_sftp_mkdir(sftp_session, ".ssh", LIBSSH2_SFTP_S_IRWXU);
    if (mkdir_rc != 0 &&
        libssh2_sftp_last_error(sftp_session) != LIBSSH2_FX_FILE_ALREADY_EXISTS) {
        goto cleanup_sftp;
    }

    {
        LIBSSH2_SFTP_ATTRIBUTES attrs;
        memset(&attrs, 0, sizeof(attrs));
        attrs.flags = LIBSSH2_SFTP_ATTR_PERMISSIONS;
        attrs.permissions = LIBSSH2_SFTP_S_IRWXU; // 0700

        // Best-effort: some SFTP servers reject setstat on directories even when
        // permissions are already correct. We don't fail the whole operation on
        // this, since the key install itself is the primary goal.
        (void)libssh2_sftp_setstat(sftp_session, ".ssh", &attrs);
    }

    {
        int present = remote_key_already_present(sftp_session, pubkey_str, pubkey_len);
        if (present == 1) {
            rc = 0;
            goto cleanup_sftp;
        } else if (present < 0) {
            goto cleanup_sftp;
        }
    }

    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(
            sftp_session,
            ".ssh/authorized_keys",
            LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_APPEND,
            LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR);

    if (!sftp_handle) goto cleanup_sftp;

    {
        size_t total_written = 0;
        int write_failed = 0;
        while (total_written < (size_t)pubkey_len) {
            ssize_t n = libssh2_sftp_write(
                    sftp_handle,
                    pubkey_str + total_written,
                    (size_t)pubkey_len - total_written);
            if (n < 0) {
                write_failed = 1;
                break;
            }
            total_written += (size_t)n;
        }
        if (!write_failed && total_written == (size_t)pubkey_len) {
            rc = 0;
        }
    }

    libssh2_sftp_close(sftp_handle);

cleanup_sftp:
    libssh2_sftp_shutdown(sftp_session);

cleanup_session:
    libssh2_session_disconnect(session, "Key Setup Complete");
    libssh2_session_free(session);
    free(pubkey_str);
    close(sock);
    return rc;
}

