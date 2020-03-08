local pktgenpath = "/usr/src/pktgen-19.12.0"
package.path = package.path .. string.format(";%s/?.lua;%s/test/?.lua",pktgenpath,pktgenpath)

require "Pktgen";

-- define packet sizes to test
local pkt_sizes		= { 64, 128, 256, 512, 1024, 1280, 1460 };

-- time in seconds to transmit for (ms)
local duration		= 10000
local pauseTime		= 2000

-- define the ports in use
local sendport		= "0"
local recvport		= "1"

-- number of repetitions
local repetitions       = 1

-- For some reason, on pktgen-19.12.0 pktgen.clr() is only resetting
-- the port stats on the screen, but not the values returned by pktgen.portStats
-- (used to work on pktgen-3.5.9), so successive calls to runThroughputTests
-- have cumulative stats. We'll have to keep track of that and subtract the
-- previous values to overcome this.
local num_rx_old = 0
local num_tx_old = 0

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
  pktgen.set(sendport, "count", 0)
  -- send at maximum rate
  pktgen.set(sendport, "rate", 100)
  -- We need to prime the underlying switch so it knows where to send
  -- packets destined to port 1
  pktgen.set(recvport, "count", 1)
  pktgen.start(recvport)
end

local function runThroughputTest(pkt_size)
  local results

  for count=1, repetitions, 1
	do
    pktgen.clr()

    -- We have to add 4 to the size because pktgen sets
    -- the size on the wire, which accounts for the L2 checksum.
    -- So to have a packet which is 64 bytes wide (of protocols and
    -- payload) we actually have to add other 4 bytes.
    pktgen.set(sendport, "size", pkt_size + 4)

    pktgen.start(sendport)
    printf("Starting trial %d/%d for size %dB\n",count, repetitions, pkt_size)
    pktgen.delay(duration)
    pktgen.stop(sendport)

    pktgen.delay(pauseTime)

    statTx = pktgen.portStats(sendport, "port")[tonumber(sendport)]
    statRx = pktgen.portStats(recvport, "port")[tonumber(recvport)]
    num_tx_raw = statTx.opackets
    num_rx_raw = statRx.ipackets
    num_tx = num_tx_raw - num_tx_old
    num_rx = num_rx_raw - num_rx_old
    num_dropped = num_tx - num_rx

    results = string.format("%d;%d;%d;%d;%d;%d",pkt_size,count,num_tx,num_rx,num_dropped,duration)
    printf("Results: %s\n",results)
    file:write(results .. "\n")

    num_tx_old = num_tx_raw
    num_rx_old = num_rx_raw

    pktgen.delay(pauseTime)
	end

end

function run()
  file = io.open("/tmp/cb/throughput.csv", "w")
  file:write("pkt size;trial number;tx packets;rx packets;dropped packets;duration(ms)\n")

  setupTraffic()
  for _,size in pairs(pkt_sizes)
  do
    runThroughputTest(size)
  end

  file:close()
  pktgen.quit()
end

run()
