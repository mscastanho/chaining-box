# Number of VMs to provision
# If N > 245, the script mus be modified
# See IP and MAC values configured below
N = 3

Vagrant.configure("2") do |config|

  (0..N-1).each do |machine_id|
    hostname = "sfc#{machine_id}"
    config.vm.define hostname do |machine|
      machine.vm.box = "generic/ubuntu1804"
      # machine.vm.box_version = "v1.8.40"
      machine.vm.hostname = hostname

      # Private network for inter-vm communication
      machine.vm.network "private_network", 
          ip: "10.0.0.#{10+machine_id}",
          mac: "00:00:00:00:00:#{"%02x" % (machine_id+10)}", # Must be lowercase
          nic_type: "virtio",
          libvirt__driver_name: "vhost",
          libvirt__driver_queues: "8"

      machine.vm.network "forwarded_port",
          guest: 22,
          host: "#{2000+machine_id}"

      machine.ssh.insert_key = false
      machine.ssh.forward_x11 = true

      machine.vm.synced_folder "src/", "/home/vagrant/dsfc/src", type: "rsync", rsync__chown: true
      machine.vm.synced_folder "test/", "/home/vagrant/dsfc/test", type: "rsync", rsync__chown: true
      machine.vm.synced_folder ".", "/vagrant", disabled: true
      
      machine.vm.provider "libvirt" do |lv|
        lv.default_prefix = ""
        lv.memory = "1024"
        lv.graphics_type = "vnc"
        lv.graphics_ip = "0.0.0.0"
      end

      # Ansible script is run only with cmds
      #     vagrant provision --provision-with ansible
      # or
      #     vagrant up --provision-with ansible
      # Check README for more details.
      if machine_id == N-1
        machine.vm.provision "ansible", type: "ansible", run: "never" do |ansible|
          ansible.limit = "all"
          ansible.verbose = "vv"
          ansible.playbook = "./ansible/provision.yml"
          ansible.extra_vars = { ansible_python_interpreter:"/usr/bin/python3" }
        end
      end

      # Hack to change VM's XML definition since we
      # cannot add some host params through vagrant-libvirt
      config.trigger.after :up do |trigger|
        trigger.info = "Adding params to XML file"
        trigger.run = {path: "./redefine-vm.sh", args: ["#{hostname}"]}
      end
    end
  end

end