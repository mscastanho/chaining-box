Vagrant.configure("2") do |config|

  config.vm.box = "ubuntu/bionic64"
  config.vm.box_version = "20181015.0.0"
  config.vm.hostname = "dsfc1"
  config.disksize.size = "100GB"
  config.vm.define "dsfc1" do |dsfc1|
  end 
  
  # Private network for inter-vm communication
  config.vm.network "private_network", ip: "10.0.0.2"

  config.ssh.insert_key = false
  config.ssh.forward_x11 = true

  config.vm.synced_folder "src/", "/home/vagrant/dsfc/src", type: "rsync", rsync__chown: true
  config.vm.synced_folder "test/", "/home/vagrant/dsfc/test", type: "rsync", rsync__chown: true
  config.vm.synced_folder ".", "/vagrant", disabled: true
  
  config.vm.provider "virtualbox" do |vb|
    vb.name = "dsfc1"
    vb.gui = false
    vb.memory = "2048"
  end

  config.vm.provision "ansible" do |ansible|
    ansible.verbose = "vv"
    ansible.playbook = "./ansible/provision.yml"
    ansible.extra_vars = { ansible_python_interpreter:"/usr/bin/python3" }
  end

end