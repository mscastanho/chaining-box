local pktgenpath = "/usr/src/pktgen-19.12.0"
package.path = package.path .. string.format(";%s/?.lua;%s/test/?.lua",pktgenpath,pktgenpath)

require "Pktgen";

-- define packet sizes to test
local pkt_sizes =  { 64, 128, 256, 512, 1024, 1280, 1460 };

-- time in seconds to transmit for (ms)
local duration = 5000;
local pauseTime = 2000;

-- define the ports in use
local sendport  = "0"
local recvport  = "1"

-- Rate to use during tests (latency measurement can't handle 10Gbps)
local send_rate = 10 -- percentage

-- number of repetitions
local repetitions = 10;

-- Cooldown and warmup times to wait while stats stabilize
local cooldown = 2000;
local warmup = 1000

-- Stats collection interval (ms)
local stats_interval = 1000

-- Output file name
local outfile = "/tmp/cb/latency.csv"

local function setupTraffic()
  local both = sendport..","..recvport
  local sendip = "10.10.0.1"
  local recvip = "10.10.0.2"

  -- Setup flow to match classification rule
  pktgen.set_ipaddr(sendport, "dst", recvip)
  pktgen.set_ipaddr(sendport, "src", sendip.."/24")
  pktgen.set_ipaddr(recvport, "dst", sendip)
  pktgen.set_ipaddr(recvport, "src", recvip.."/24")
  pktgen.set(both, "sport", 1000)
  pktgen.set(both, "dport", 2000)
  pktgen.set_proto(both, "udp")

  -- set Pktgen to send continuous stream of traffic
  pktgen.set(sendport, "count", 0);

  -- send at maximum rate
  pktgen.set(sendport, "rate", send_rate)

  -- Enable latency measurement on both ports
  pktgen.latency(sendport, 'enable');
  pktgen.latency(recvport, 'enable');

  -- We need to prime the underlying switch so it knows where to send
  -- packets destined to port 1
  pktgen.set(recvport, "count", 1)
  pktgen.start(recvport)
end

local function runLatencyTest(pkt_size)
	local num_dropped, max_rate, min_rate, trial_rate;
  local results;

	for count=1, repetitions, 1
	do
    pktgen.clr();

    -- We have to add 4 to the size because pktgen sets
    -- the size on the wire, which accounts for the L2 checksum.
    -- So to have a packet which is 64 bytes wide (of protocols and
    -- payload) we actually have to add other 4 bytes.
    pktgen.set(sendport, "size", pkt_size + 4);

    pktgen.start(sendport);
    printf("Starting trial %d/%d for size %dB\n",count,repetitions,pkt_size)
    pktgen.delay(duration);
    pktgen.stop(sendport);

    pktgen.delay(pauseTime);

    statTx  = pktgen.portStats(sendport, "port")[tonumber(sendport)];
    statRx  = pktgen.portStats(recvport, "port")[tonumber(recvport)];
    num_tx  = statTx.opackets;
    num_rx  = statRx.ipackets;

    statPkt = pktgen.pktStats("all")[tonumber(recvport)]

    min_lat = statPkt.min_latency
    avg_lat = statPkt.avg_latency
    max_lat = statPkt.max_latency

    results = string.format("%d,%d,%d,%d,%d,%d,%d,%d",pkt_size,count,num_tx,num_rx,min_lat,avg_lat,max_lat,duration);

    printf("Results: %s\n",results);
    file:write(results .. "\n");
    pktgen.delay(pauseTime);
	end
end

function run()
  file = io.open(outfile, "w");
  file:write("pkt size,trial number,tx packets,rx packets,min latency,avg latency,max latency,duration(ms)\n")
  setupTraffic();
  pktgen.page("latency")
  for _,size in pairs(pkt_sizes)
  do
    runLatencyTest(size);
  end

  file:close();
  pktgen.quit();
end

run();

