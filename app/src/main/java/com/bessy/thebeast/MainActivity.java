package com.bessy.thebeast;

import android.app.Activity;
import android.os.Bundle;
import android.text.InputType;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.Toast;

public class MainActivity extends Activity {

    static {
        System.loadLibrary("native_core");
    }

    private native int nativeSaveConfigAndSetupKey(
            String targetIp,
            String macAdder,
            String broadcastIp,
            String userName,
            String password,
            String keyPath,
            String config_path
    );

    private native String[] nativeLoadConfig(String configPath);

    private EditText ipInput;
    private EditText macInput;
    private EditText broadcastIpInput;
    private EditText userNameInput;
    private EditText passwordInput;
    private Button saveBtn;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        saveBtn = new Button(this);

        ScrollView scrollView = new ScrollView(this);
        LinearLayout layout = new LinearLayout(this);
        layout.setOrientation(LinearLayout.VERTICAL);
        int padding = (int) (16 * getResources().getDisplayMetrics().density);
        layout.setPadding(padding, padding, padding, padding);

        createAllFields();
        saveBtn.setText("Save config and Exchange SSH Key");

        LinearLayout.LayoutParams btnParams = new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT
        );
        btnParams.setMargins(0, padding, 0, 0);
        saveBtn.setLayoutParams(btnParams);

        saveBtn.setOnClickListener(v -> handleSave());

        setAllLayout(layout);

        scrollView.addView(layout);
        setContentView(scrollView);

        loadExistingConfig();
    }

    private EditText createField(String hint, int inputType) {
        EditText field = new EditText(this);
        field.setHint(hint);
        field.setInputType(inputType);
        return field;
    }

    private void createAllFields() {
        ipInput = createField("Target Server IP (SSH)", InputType.TYPE_CLASS_TEXT);
        userNameInput = createField("SSH Username", InputType.TYPE_CLASS_TEXT);
        macInput = createField(
                "Target MAC Address (AA:BB:CC:DD:EE:FF)",
                InputType.TYPE_CLASS_TEXT
        );
        broadcastIpInput = createField(
                "Broadcast IP (eg., 192.168.1.255)",
                InputType.TYPE_CLASS_TEXT
        );
        passwordInput = createField(
                "One-Time SSH Password",
                InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD
        );
    }

    private void setAllLayout(LinearLayout layout) {
        layout.addView(ipInput);
        layout.addView(macInput);
        layout.addView(broadcastIpInput);
        layout.addView(userNameInput);
        layout.addView(passwordInput);
        layout.addView(saveBtn);
    }

    private void handleSave() {
        String ip = ipInput.getText().toString().trim();
        String macAdder = macInput.getText().toString().trim();
        String broadcastIp = broadcastIpInput.getText().toString().trim();
        String userName = userNameInput.getText().toString().trim();
        String password = passwordInput.getText().toString().trim();

        if (ip.isEmpty() || macAdder.isEmpty() || broadcastIp.isEmpty() || userName.isEmpty()) {
            Toast.makeText(
                    this,
                    "Please fill in all target parameters",
                    Toast.LENGTH_SHORT
            ).show();
            return;
        }

        String keyPath = getFilesDir().getAbsolutePath() + "/id_ed25519";
        String configurePath = getFilesDir().getAbsolutePath() + "/config.bin";

        new Thread(() -> {

        });
    }

    private void loadExistingConfig() {

    }
}
