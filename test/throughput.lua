local pktgenpath = "/usr/src/pktgen-19.12.0"
package.path = package.path .. string.format(";%s/?.lua;%s/test/?.lua",pktgenpath,pktgenpath)

require "Pktgen";

-- time in seconds to transmit for (ms)
local duration		= 10000
local pauseTime		= 2000

-- define the ports in use
local sendport		= "0"
local recvport		= "1"

-- number of repetitions
local repetitions       = 1

local function setupTraffic()
	pktgen.set_proto(sendport..","..recvport, "udp")
  -- set Pktgen to send continuous stream of traffic
  pktgen.set(sendport, "count", 0)
  -- send at maximum rate
  pktgen.set(sendport, "rate", 100)
  -- We need to prime the underlying switch so it knows where to send
  -- packets destined to port 1
  pktgen.set(recvport, "count", 1)
  pktgen.start(recvport)
end

local function runThroughputTest()
	local num_dropped, max_rate, min_rate, trial_rate
  local results

  for count=1, repetitions, 1
	do
    pktgen.clr()

    pktgen.start(sendport)
    printf("Starting trial %d/%d\n",count,repetitions)
    pktgen.delay(duration)
    pktgen.stop(sendport)

    pktgen.delay(pauseTime)

    statTx = pktgen.portStats(sendport, "port")[tonumber(sendport)]
    statRx = pktgen.portStats(recvport, "port")[tonumber(recvport)]
    num_tx = statTx.opackets
    num_rx = statRx.ipackets
    num_dropped = num_tx - num_rx

    results = string.format("%d;%d;%d;%d;%d",count,num_tx,num_rx,num_dropped,duration)

    printf("Results: %s\n",results)
    file:write(results .. "\n")
    pktgen.delay(pauseTime)
	end

end

function run()
  file = io.open("/tmp/cb/tput.res", "w")
  file:write("trial number;tx packets;rx packets;dropped packets;duration(ms)\n")

  setupTraffic()
  runThroughputTest()

  file:close()
  pktgen.quit()
end

run();
