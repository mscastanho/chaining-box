# Download kernel 4.18
cd /usr/src
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.18/linux-headers-4.18.0-041800_4.18.0-041800.201808122131_all.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.18/linux-headers-4.18.0-041800-generic_4.18.0-041800.201808122131_amd64.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.18/linux-image-4.18.0-041800-generic_4.18.0-041800.201808122131_arm64.deb
wget http://kernel.ubuntu.com/~kernel-ppa/mainline/v4.18/linux-modules-4.18.0-041800-generic_4.18.0-041800.201808122131_amd64.deb
sudo dpkg -i *4.18.0*.deb

# sudo reboot