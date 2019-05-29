# Do all VM creation in series.
# Parallel actions seem to break things.
ENV['VAGRANT_NO_PARALLEL'] = 'yes'

# Number of VMs to provision
# If N > 245, the script mus be modified
# See IP and MAC values configured below
N = 2

Vagrant.configure("2") do |config|

  (0..N-1).each do |machine_id|
    hostname = "sfc#{machine_id}"
    config.vm.define hostname do |machine|
      machine.vm.box = "mscastanho/bpf-sandbox"
      machine.vm.box_version = "0.4"
      machine.vm.hostname = hostname

      # Machine specs
      machine.vm.provider "libvirt" do |lv|
        lv.default_prefix = ""
        lv.memory = "1024"
        lv.graphics_type = "vnc"
        lv.graphics_ip = "0.0.0.0"
      end

      # Private network for inter-vm communication
      machine.vm.network "private_network",
          ip: "10.10.10.#{10+machine_id}",
          mac: "00:00:00:00:00:#{"%02x" % (machine_id+10)}", # Must be lowercase
          nic_type: "virtio",
          libvirt__driver_name: "vhost",
          libvirt__driver_queues: "8"

      machine.vm.network "forwarded_port",
          guest: 22,
          host: "#{2000+machine_id}"

      machine.ssh.insert_key = false
      machine.ssh.forward_x11 = true

      # Configure synced folders
      machine.vm.synced_folder "headers/", "/home/vagrant/chaining-box/headers", type: "rsync", rsync__chown: true
      machine.vm.synced_folder "src/", "/home/vagrant/chaining-box/src", type: "rsync", rsync__chown: true
      machine.vm.synced_folder "test/", "/home/vagrant/chaining-box/test", type: "rsync", rsync__chown: true
      machine.vm.synced_folder "libbpf/", "/home/vagrant/chaining-box/libbpf", type: "rsync", rsync__chown: true
      machine.vm.synced_folder ".", "/vagrant", disabled: true

      # Install missing packages
      # Renew DHCP client to get new IP
      machine.vm.provision "shell",
        inline: "sudo apt install bpfcc-tools python3-bpfcc python3-pyroute2 -y && sudo dhclient eth0"
    end
  end
end