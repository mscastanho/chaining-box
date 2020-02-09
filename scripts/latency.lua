local pktgenpath = "/usr/src/pktgen-3.5.9"
package.path = package.path .. string.format(";%s/?.lua;%s/test/?.lua",pktgenpath,pktgenpath)

require "Pktgen";

-- define the ports in use
local sendport  = "0"
local recvport  = "1"

-- Rate to use during tests (latency measurement can't handle 10Gbps)
local send_rate = 10 -- percentage

-- define packet sizes to test
local pkt_sizes =  { 64, 128, 256, 512, 1024, 1280, 1500 };

-- time in seconds to transmit for (ms)
local duration = 5000;
local pauseTime = 2000;

-- number of repetitions
local repetitions = 10;

-- Cooldown and warmup times to wait while stats stabilize
local cooldown = 2000;
local warmup = 1000

-- Stats collection interval (ms)
local stats_interval = 1000

-- Output file name
local outfile = "latency.csv"

local function setupTraffic()
    pktgen.set_proto(sendport..","..recvport, "udp");

    -- set Pktgen to send continuous stream of traffic
    pktgen.set(sendport, "count", 0);

    -- send at maximum rate
    pktgen.set(sendport, "rate", send_rate)

    -- Enable latency measurement on both ports
    pktgen.latency(sendport, 'enable');
    pktgen.latency(recvport, 'enable');
end

local function runLatencyTest(pkt_size)
	local num_dropped, max_rate, min_rate, trial_rate;
   local results;

	for count=1, repetitions, 1
	do
        pktgen.clr();

        -- To understand why the +4, check the comment
        -- on throughput.lua script
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
    printf("What's the output file name? ")
    filename = ""
    c = ""
    while c ~= "\n" do
        c = io.read(1)
        if c ~= "\n" then
            filename = filename..c
        end
        printf("%s",c)
    end

    file = io.open(filename, "w");
    file:write("pkt size,trial number,tx packets,rx packets,min latency,avg latency,max latency,duration(ms)\n")
    setupTraffic();
    pktgen.page("latency")
    for _,size in pairs(pkt_sizes)
    do
      runLatencyTest(size);
    end
    file:close();
    -- pktgen.quit();
end

run();

