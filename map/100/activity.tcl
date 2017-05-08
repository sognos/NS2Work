set val(chan)           Channel/WirelessChannel    ;# channel type
set val(prop)           Propagation/TwoRayGround   ;# radio-propagation model
set val(netif)          Phy/WirelessPhy           ;# network interface type
set val(mac)            Mac/802_11                 ;# MAC type
set val(ifq)            Queue/DropTail/PriQueue    ;# interface queue type
set val(ll)             LL                         ;# link layer type
set val(ant)            Antenna/OmniAntenna        ;# antenna model
set val(ifqlen)         50                         ;# max packet in ifq
set val(nn)             101                        ;# number of mobilenodes
set val(rp)             DYMOUM                       ;# routing protocol
# set start/stop time
set val(start) 0
set val(stop) 200
set val(x) 2305
set val(y) 2440
# ======================================================================
# Main Program
# ======================================================================


#
# Initialize Global Variables
#
set ns_		[new Simulator]
set tracefd     [open simple.tr w]
$ns_ trace-all $tracefd

set namf [open guindy.nam w]
$ns_ namtrace-all-wireless $namf $val(x) $val(y)

# set up topography object
set topo       [new Topography]
$topo load_flatgrid $val(x) $val(y)
# Create God
create-god $val(nn)

#
#  Create the specified number of mobilenodes [$val(nn)] and "attach" them
#  to the channel. 
#  

# configure node

        $ns_ node-config -adhocRouting $val(rp) \
			 -llType $val(ll) \
			 -macType $val(mac) \
			 -ifqType $val(ifq) \
			 -ifqLen $val(ifqlen) \
			 -antType $val(ant) \
			 -propType $val(prop) \
			 -phyType $val(netif) \
			 -channel [new $val(chan)]  \
			 -topoInstance $topo \
			 -agentTrace    ON \
			 -routerTrace   OFF \
			 -macTrace      OFF \
			 -movementTrace OFF \
                         
                         
Agent/DYMOUM set debug_ false
Agent/DYMOUM set reissue_rreq_ true
Agent/DYMOUM set hello_ival_ 0
Agent/DYMOUM set s_bit_ false
Agent/DYMOUM set no_path_acc_ true
			 
	for {set i 0} {$i < $val(nn) } {incr i} {

		set node_($i) [$ns_ node]	
		$node_($i) random-motion 0		;# disable random motion
		$ns_ initial_node_pos $node_($i) 20
	}

#
# Provide initial (X,Y, for now Z=0) co-ordinates for mobilenodes
#
source mobility.tcl
#$ns_ at 1.8 "[$node_(11) set ragent_] blackhole"
#$ns_ at 0.0 "[$node_(1) set ragent_] blackhole"
#$ns_ at 0.0 "[$node_(40) set ragent_] blackhole"



source traffic.tcl

#
# Tell nodes when the simulation ends
#
for {set i 0} {$i < $val(nn) } {incr i} {
    $ns_ at $val(stop) "$node_($i) reset";
}
$ns_ at $val(stop) "stop"
$ns_ at $val(stop) "puts \"NS EXITING...\" ; $ns_ halt"

proc stop {} {
    global ns_ tracefd
    $ns_ flush-trace
    close $tracefd
}

puts "Starting Simulation..."
$ns_ run