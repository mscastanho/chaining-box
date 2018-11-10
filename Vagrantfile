hosts = {
  "classifier" => "10.0.0.10",
  "dsfc1" => "10.0.0.11",
  "dsfc2" => "10.0.0.12",
  "dsfc3" => "10.0.0.13"
}

Vagrant.configure("2") do |config|

  hosts.each do |hostname,ip| 
    config.vm.define hostname do |machine|
      machine.vm.box = "ubuntu/bionic64"
      machine.vm.box_version = "20181015.0.0"
      machine.vm.hostname = hostname
      # machine.disksize.size = "100GB"

      # Private network for inter-vm communication
      machine.vm.network "private_network", ip: ip, nic_type: "virtio"

      machine.ssh.insert_key = false
      machine.ssh.forward_x11 = true

      machine.vm.synced_folder "src/", "/home/vagrant/dsfc/src", type: "rsync", rsync__chown: true
      machine.vm.synced_folder "test/", "/home/vagrant/dsfc/test", type: "rsync", rsync__chown: true
      machine.vm.synced_folder ".", "/vagrant", disabled: true
      
      machine.vm.provider "virtualbox" do |vb|
        vb.name = hostname
        vb.gui = false
        vb.memory = "2048"
      end

      machine.vm.provision "ansible" do |ansible|
        ansible.verbose = "vv"
        ansible.playbook = "./ansible/provision.yml"
        ansible.extra_vars = { ansible_python_interpreter:"/usr/bin/python3" }
      end
    end
  end

end