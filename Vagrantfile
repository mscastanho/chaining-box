Vagrant.configure("2") do |config|

  config.vm.box = "ubuntu/bionic64"
  config.vm.hostname = "dsfc1"
  
  # Private network for inter-vm communication
  config.vm.network "private_network", ip: "10.0.0.2"

  # Network for communication with host
  # config.vm.network "host_network", ip: fixed!!

  config.vm.provider "virtualbox" do |vb|
    # Display the VirtualBox GUI when booting the machine
    vb.gui = false
    vb.memory = "2048"
  end

  # config.vm.provision "shell", path: "provision.sh"
end
