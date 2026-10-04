#!/bin/bash
# Slipper System Installer & Updater

echo "--- Building Slipper Architecture ---"
./build.sh

if [ $? -ne 0 ]; then
    echo "[!] Build failed. Aborting installation."
    exit 1
fi

echo "--- Stopping Slipper Daemon (if running) ---"
if [ -f "/etc/init.d/slipperd" ]; then
    sudo rc-service slipperd stop 2>/dev/null
fi

echo "--- Establishing Privilege Separation ---"
sudo mkdir -p /run/slipper
sudo chown root:wheel /run/slipper
sudo chmod 770 /run/slipper

if [ -f "build/slipperd" ]; then
    sudo cp build/slipperd /usr/local/sbin/slipperd
    sudo chmod 700 /usr/local/sbin/slipperd
    echo "Updated backend daemon: /usr/local/sbin/slipperd"
fi

if [ -f "build/slip" ]; then
    sudo cp build/slip /usr/local/bin/slip
    sudo chmod 755 /usr/local/bin/slip
    echo "Updated user CLI: /usr/local/bin/slip"
fi

echo "--- Deploying Native Bash Environment ---"
sudo mkdir -p /usr/local/share/slipper
sudo cp src/execution/slipper-functions.sh /usr/local/share/slipper/
sudo chmod +x /usr/local/share/slipper/slipper-functions.sh

echo "--- Installing OpenRC Init Script ---"
sudo cat << 'EOF' > /tmp/slipperd.init
#!/sbin/openrc-run

name="Slipper Daemon"
command="/usr/local/sbin/slipperd"
command_background="yes"
pidfile="/run/slipper/slipperd.pid"

depend() {
    need localmount
}

start_pre() {
    checkpath -d -m 0770 -o root:wheel /run/slipper
}
EOF

sudo mv /tmp/slipperd.init /etc/init.d/slipperd
sudo chmod +x /etc/init.d/slipperd

echo "--- Starting Slipper Daemon ---"
sudo rc-service slipperd start

echo "Installation Complete! Run 'slip -avuD @world' to test."