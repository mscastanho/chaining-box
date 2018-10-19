Vagrant.configure("2") do |config|

  config.vm.box = "ubuntu/bionic64"
  config.vm.hostname = "dsfc1"
  config.vm.define "dsfc1" do |dsfc1|
  end 

  # Private network for inter-vm communication
  config.vm.network "private_network", ip: "10.0.0.2"

  # Network for communication with host
  # config.vm.network "host_network", ip: fixed!!

  config.ssh.insert_key = false

  config.vm.provider "virtualbox" do |vb|
    # Display the VirtualBox GUI when booting the machine
    vb.name = "dsfc1"
    vb.gui = false
    vb.memory = "2048"
  end

  config.vm.provision "ansible" do |ansible|
    ansible.playbook = "provision.yml"
    ansible.extra_vars = { ansible_python_interpreter:"/usr/bin/python3" }
  end

  # config.vm.provision "shell", path: "provision.sh"
end