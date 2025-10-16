# optionsArr is filled as a map from option names (e.g. '-help') to a list of instances of this option.
# Each instance is a list of arguments passed to the option.
proc parseOptions {argsList} {
	upvar optionsArr optionsArr
	array set optionsArr {}

	set argId 0
	set currOptionName ""
	set currOptionArgs {}
	
  	foreach arg $argsList {
		if {[string match "-*" $arg]} {
			# found a new option
			if {$currOptionName != ""} {
				# add current option to array, and clear current option
				lappend optionsArr($currOptionName) $currOptionArgs
				set currOptionName ""
				set currOptionArgs {}
			}
			set currOptionName $arg
		} else {
			# found a new arg
			if {$currOptionName != ""} {
				# valid option exists - add arg to option args list
				lappend currOptionArgs $arg
			} else {
				# no valid option - return an error
				error "ERROR: invalid option name ($arg) - must start with '-'"
			}
		}
	}

	# add last option, if any
	if {$currOptionName != ""} {
		lappend optionsArr($currOptionName) $currOptionArgs
	}
}

# verifies and returns the value of the specified option from optionsArr
# expectedNumArgs: expected number of arguments (if scalar), or expected arguments range (if {min max} list)
# expectedNumInstances: expected number of instances (if scalar), or expected instances range (if {min max} list)
# if command is a lambda function, call it for each instance with the selected arguments as parameters.
# if command is omitted, there can be at most one instance, and the return value depends on the number of arguments:
#	0 args: return 1 if option appeared, 0 otherwise.
#	1 arg: return it. 
#	args > 1: return the args list
# if option was not found return 0

proc handleParseOption {optionName {expectedNumArgs 0} {expectedNumInstances {0 1}} {command 0}} {
	upvar optionsArr optionsArr	
	
	set instancesList {}
	if {[info exists optionsArr($optionName)]} {		
		set instancesList $optionsArr($optionName)
	    unset optionsArr($optionName)		
	}
	set numInstances [llength $instancesList]
	if {[llength $expectedNumInstances] > 1} {
        if {[llength $expectedNumInstances] != 2} {
		    error "ERROR: number of expected instances ($expectedNumInstances) must be a scalar or a list of size 2"
        }
		set minInstances [lindex $expectedNumInstances 0]
		set maxInstances [lindex $expectedNumInstances 1]
		if {$numInstances < $minInstances} {
			error "ERROR: $optionName is specified $numInstances times, but should be specified at least $minInstances times"
		}
		if {$numInstances > $maxInstances} {
			error "ERROR: $optionName is specified $numInstances times, but should be specified at most $maxInstances times"
		}
	} else {
		if {$numInstances != $expectedNumInstances} {
			error "ERROR: $optionName is specified $numInstances times, but should be specified exactly $expectedNumInstances times"
		}
	}

	if {$numInstances > 1 && $command == 0} {
		error "ERROR: $optionName was specified multiple times and therefore must be associated with a command"
	}

	set result 0

    foreach argsList $instancesList {
		set numArgs [llength $argsList]
		if {[llength $expectedNumArgs] > 1} {
            if {[llength $expectedNumArgs] != 2} {
		        error "ERROR: number of expected arguments ($expectedNumArgs) must be a scalar or a list of size 2"
            }
			set minArgs [lindex $expectedNumArgs 0]
			set maxArgs [lindex $expectedNumArgs 1]
			if {$numArgs < $minArgs} {
				error "ERROR: $optionName has $numArgs arguments, but should have at least $minArgs arguments"
			}
			if {$numArgs > $maxArgs} {
				error "ERROR: $optionName has $numArgs arguments, but should have at most $maxArgs arguments"
			}
		} else {
			if {$numArgs != $expectedNumArgs} {
				error "ERROR: $optionName has $numArgs arguments, but should have $expectedNumArgs arguments"
			}
		}
        if {$command == 0} {			
	        switch $numArgs {
		        0 {return 1}
		        1 {return [lindex $argsList 0]}
		        default {return $argsList}
	        }
        } else {
	        set result [apply $command {*}$argsList]
        }
	}

	return $result
}

########################################### Debug Bus #################################################

set _dbgConfigHelp    "Configures the Debug Bus. Should be followed by dbgStart and dbgDump.\n"
append _dbgConfigHelp "Usage: dbgConfig <options>\n"
append _dbgConfigHelp "where <options> is any of the following:\n\n"
append _dbgConfigHelp "-oneShot\n"
append _dbgConfigHelp "    Sets 'one shot' recording mode, in which the recording stops when the end of\n"
append _dbgConfigHelp "    the buffer is reached. If omitted, 'wrap around' mode is used instead.\n\n"
append _dbgConfigHelp "-pci <bufSizeKB>\n"
append _dbgConfigHelp "    Directs the debug output to a PCI buffer with the specified size.\n\n"
append _dbgConfigHelp "-nw <portId> \[<destMacAddr> <dataLimitSizeKB> <otherEngineDevId\]\n"
append _dbgConfigHelp "    Directs the debug output to the specified NW port.\n"
append _dbgConfigHelp "    <destMacAddr>: destination MAC address in the form ##:##:##:##:##:##.\n"
append _dbgConfigHelp "        Default is 00:50:C2:2C:71:9C.\n"
append _dbgConfigHelp "    <dataLimitSizeKB>: Tx data limit size in KB (valid only for one-shot mode).\n"
append _dbgConfigHelp "        If set to 0, Tx data isn't limited. Default is 0.\n"
append _dbgConfigHelp "    <otherEngineDevId>: if specified, the debug output is transmitted from the\n"
append _dbgConfigHelp "        NW port of the other engine (the specified device is assumed to be\n"
append _dbgConfigHelp "        loaded). If omitted, the current engine is used.\n\n"
append _dbgConfigHelp "-rh <stormLetters>\n"
append _dbgConfigHelp "    Enables the specified Storms for Recording Handlers.\n"
append _dbgConfigHelp "    <stormLetters>: a string containing the letters x/y/p/t/m/u (e.g. -rh txu).\n\n"
append _dbgConfigHelp "-semFilter <stormLetter> eid range <minEid> <maxEid>\n"
append _dbgConfigHelp "                         eid mask <eidVal> <eidMask>\n"
append _dbgConfigHelp "                         cid <cid>\n"
append _dbgConfigHelp "    Applies the specified SEM filter.\n"
append _dbgConfigHelp "    EID range filter: records only data that belongs to handlers with event IDs\n"
append _dbgConfigHelp "        in the specified range <minEid>..<maxEid>.\n"
append _dbgConfigHelp "    EID mask filter: records only data that belongs to handlers with event IDs\n"
append _dbgConfigHelp "        that match <eidVal>. Only bit indexes in which <eidMask> contain 1's\n"
append _dbgConfigHelp "        are compared.\n"
append _dbgConfigHelp "    CID filter: records only data that belongs to handlers with the specified\n"
append _dbgConfigHelp "        CID <cid>.\n\n"
append _dbgConfigHelp "-storm <stormLetter> <mode>\n"
append _dbgConfigHelp "    Enables the specified Storm for recording, and configures the recording\n"
append _dbgConfigHelp "    mode. Can be specified multiple times for different Storms.\n"
append _dbgConfigHelp "    <stormLetter>: x/y/p/t/m/u.\n"
append _dbgConfigHelp "    <mode>:\n"
append _dbgConfigHelp "        rh -             Recording Handlers (compressed in E5 by default)\n"
append _dbgConfigHelp "        rh_no_compress - Recording Handlers without compression (E5 only)\n"
append _dbgConfigHelp "        rh_with_store  - Recording Handlers with STORE messages\n"
append _dbgConfigHelp "        printf -         data stored by FW to 0xD000 and 0xD001\n"
append _dbgConfigHelp "        pram_addr -      PRAM addresses\n"
append _dbgConfigHelp "        dra_rw -         DRA read/write (E4 only)\n"
append _dbgConfigHelp "        dra_w -          DRA write and new thread fields (E4 only)\n"
append _dbgConfigHelp "        ld_st_addr -     LOAD/STORE addresses (E4 only)\n"
append _dbgConfigHelp "        dra_fsm -        DRA fast state machine\n"
append _dbgConfigHelp "        fast_dbgmux -    SEM_FAST DBGMUX (E5 only)\n"
append _dbgConfigHelp "        foc -            FOC messages (slow debug channel, E4 only)\n"
append _dbgConfigHelp "        ext_store -      external STORE messages (slow debug channel, E4 only)\n\n"
append _dbgConfigHelp "-block <block> <line> \[<enableMask> <rightShift> <forceValidMask>\n"
append _dbgConfigHelp "       <forceFrameMask>\]\n"
append _dbgConfigHelp "    Enables the specified block for recording. Can be specified multiple times\n"
append _dbgConfigHelp "    for different blocks.\n"
append _dbgConfigHelp "    <block>: block name, one of the following: grc, pglue_b, cnig, ncsi, bmb,\n"
append _dbgConfigHelp "             pcie, mcp, pswhst, pswhst2, pswrd, pswrd2, pswwr, pswwr2, pswrq,\n"
append _dbgConfigHelp "             pswrq2, pglcs, dmae, ptu, *cm, qm, tm, dorq, brb, src, prs,\n"
append _dbgConfigHelp "             prs_fc\[_a|_b\], *sdm, *sem\[_hc\], rss, tmld, muld, yuld, xyld,\n"
append _dbgConfigHelp "             prm, pbf_pb1, pbf_pb2, rpb, btb, pbf, pbf_fc\[_b\], rdif, tdif,\n"
append _dbgConfigHelp "             cdu, ccfc, tcfc, igu, cau, umac, wol, bmbn, nwm, nws, ms,\n"
append _dbgConfigHelp "             phy_pcie, nig, nig_(rx|tx|lb)\[_fc\]\[_fc_pllh\], \n"
append _dbgConfigHelp "    <line>: block debug line. Can be specified as a line name (taken from the\n"
append _dbgConfigHelp "            block's debug Excel file) or as a line number (taken from the\n"
append _dbgConfigHelp "            block's generated debug verilog file). Special debug lines:\n"
append _dbgConfigHelp "            - signature (line #0): block's signature, exists in all blocks.\n"
append _dbgConfigHelp "            - latency (line #1): block's latency events, exists in some blocks.\n"
append _dbgConfigHelp "    <enableMask>: 4-bit value. If bit i is set, unit i in each cycle is enabled.\n"
append _dbgConfigHelp "        Default is 0xf (enables all units). A unit is 32 bits (for 128-bit debug\n"
append _dbgConfigHelp "        lines) or 64 bits (for 256-bit debug lines).\n"
append _dbgConfigHelp "    <rightShift>: number of units to cyclic right shift the debug line (0-3).\n"
append _dbgConfigHelp "        Default is 0 (no shift).\n"
append _dbgConfigHelp "    <forceValidMask>: 4-bit value. If bit i is set, unit i in each cycle is\n"
append _dbgConfigHelp "        forced valid. Default is 0 (no forced valid).\n"
append _dbgConfigHelp "    <forceFrameMask>: 4-bit value. If bit i is set, the frame bit of unit i in\n"
append _dbgConfigHelp "        each cycle is forced. Default is 0 (no forced frame bit).\n\n"
append _dbgConfigHelp "-timestamp \[<validEnable> <frameEnable> <tickLen>\]\n"
append _dbgConfigHelp "    Enables timestamp recording. The timestamp can be recorded only to dword 0\n"
append _dbgConfigHelp "    with HW ID 0.\n"
append _dbgConfigHelp "    <validEnable>/<frameEnable>: a bit mask value, containing an enable bit for\n"
append _dbgConfigHelp "        each dword (In BB/AH: 3 bits for dwords 1..3, in E5: 7 bits for dwords\n"
append _dbgConfigHelp "        1..7). The timestamp is recorded if validEnable\[i\] is set and dword\n"
append _dbgConfigHelp "        i+1 is valid, or if frameEnable\[i\] is set and the frame bit of dword\n"
append _dbgConfigHelp "        i+1 is set. By default, all bits are set.\n"
append _dbgConfigHelp "    <tickLen>: timestamp tick length in cycles. Default is 1.\n\n"
append _dbgConfigHelp "-filter \[<constMsgLen>\] <constraint1> \[<constraint2> <constraint3>\n"
append _dbgConfigHelp "        <constraint4>\]\n"
append _dbgConfigHelp "    Configures the recording filter. The filter contains up to 4 constraints.\n"
append _dbgConfigHelp "    The data is 'filtered in' when all 'must' constraints hold AND at least one\n"
append _dbgConfigHelp "    'non-must' constraint (if added) hold.\n"
append _dbgConfigHelp "    <constMsgLen>: Constant message length (in cycles) to be used for message\n"
append _dbgConfigHelp "        based filter constraints. If set to 0, message length is based only on\n"
append _dbgConfigHelp "        frame bit received from HW (no constant message length). If omitted,\n"
append _dbgConfigHelp "        default is 1 when filtering on HW blocks, and 0 when filtering on Storms.\n"
append _dbgConfigHelp "    <constraint#>: a TCL list of the following form:\n"
append _dbgConfigHelp "        {<must> <op> <data> <dataMask> \[<dwordOffset> <cycleOffset>\n"
append _dbgConfigHelp "         <compareFrame> <frameBit>\]}\n"
append _dbgConfigHelp "        <must>: indicates if this constraint is a 'must' (1) or not (0).\n"
append _dbgConfigHelp "        <op>: eq/ne/lt/ltc/le/lec/gt/gtc/ge/gec.\n"
append _dbgConfigHelp "        <data>: 32-bit data to compare with the recorded data.\n"
append _dbgConfigHelp "        <dataMask>: 32-bit mask for data comparison. If mask bit i is 1, data\n"
append _dbgConfigHelp "            bit i is compared, otherwise it's ignored. If <op> is not eq/ne,\n"
append _dbgConfigHelp "            the mask must be non-zero, and all its 1's must be continuous.\n"
append _dbgConfigHelp "        <dwordOffset>: offset in dwords from the beginning of a cycle\n"
append _dbgConfigHelp "            (0-3 in BB/AH, 0-7 in E5). If omitted, default is 0.\n"
append _dbgConfigHelp "        <cycleOffset>: offset in cycles from the beginning of a message.\n"
append _dbgConfigHelp "            If omitted, default is 0.\n"
append _dbgConfigHelp "        <compareFrame>: indicates if the frame bit should be compared (0/1).\n"
append _dbgConfigHelp "            Must be false if <op> is not eq/ne. If omitted, default is 0.\n"
append _dbgConfigHelp "        <frameBit>: frame bit to compare with the recorded data (0/1).\n"
append _dbgConfigHelp "            Should be specified only if compareFrame is 1.\n\n"
append _dbgConfigHelp "-trigger \[<recPre> <preChunks> <recPost> <postCycles> <filterPre> <filterPost>\]\n"
append _dbgConfigHelp "         <state1> \[<state2> <state3>\]\n"
append _dbgConfigHelp "    Configures the recording trigger. The trigger contains up to 3 states, where\n" 
append _dbgConfigHelp "    each state contains up to 4 constraints. After the constraints of a state\n"
append _dbgConfigHelp "    hold for the specified number of times, the next state is reached. If\n"
append _dbgConfigHelp "    there's no next state, the DBG block triggers.\n"
append _dbgConfigHelp "    <recPre>: indicates if the recording should start before the trigger (1) or\n"
append _dbgConfigHelp "        at the trigger (0). If omitted, default is 0.\n"
append _dbgConfigHelp "    <preChunks>: max number of chunks to record before the trigger (1-47). If\n"
append _dbgConfigHelp "        set to 0, recording starts from time 0. Ignored if <recPre> is set to 0.\n"
append _dbgConfigHelp "        If omitted, default is 0.\n"
append _dbgConfigHelp "    <recPost>: indicates if the recording should end after the trigger (1) or\n"
append _dbgConfigHelp "        at the trigger (0). If omitted, default is 1.\n"
append _dbgConfigHelp "    <postCycles>: number of cycles to record after the trigger (0x1-0xffffffff).\n"
append _dbgConfigHelp "        If set to 0, recording ends only when stopped by the user. Ignored if\n"
append _dbgConfigHelp "        <recPost> is set to 0. If omitted, default is 0.\n"
append _dbgConfigHelp "    <filterPre>: indicates if data should be filtered before the trigger (0/1).\n"
append _dbgConfigHelp "        Ignored if the filter wasn't enabled. If omitted, default is 0.\n"
append _dbgConfigHelp "    <filterPost>: indicates if data should be filtered after the trigger (0/1).\n"
append _dbgConfigHelp "        Ignored if the filter wasn't enabled. If omitted, default is 1.\n"
append _dbgConfigHelp "    <state#>: a TCL list of the following form:\n"
append _dbgConfigHelp "        {\[<stormLetter>\] \[<constMsgLen> <countToNext>\] <constraint1> \[<constraint2>\n"
append _dbgConfigHelp "         <constraint3> <constraint4>\]}\n"
append _dbgConfigHelp "        <stormLetter>: Storm letter (x/y/p/t/m/u), to be specified only when triggering\n"
append _dbgConfigHelp "            on Storm data. Must be omitted when triggering on HW blocks (the block to\n"
append _dbgConfigHelp "            trigger on is determined automatically based on the trigger constraints)\n"
append _dbgConfigHelp "        <constMsgLen>: Constant message length (in cycles) to be used for\n"
append _dbgConfigHelp "            message-based trigger constraints. If set to 0, message length is\n"
append _dbgConfigHelp "            based only on frame bit received from HW (no constant message\n"
append _dbgConfigHelp "            length). If omitted, default is 1 when triggering on HW blocks, and 0 when\n"
append _dbgConfigHelp "            triggering on Storms.\n"
append _dbgConfigHelp "        <countToNext>: The number of times the constraints of the state should\n"
append _dbgConfigHelp "            hold before moving to the next state. Must be non-zero. If omitted,\n"
append _dbgConfigHelp "            default is 1.\n"
append _dbgConfigHelp "        <constraint#>: see description in -filter section.\n\n"
append _dbgConfigHelp "-grc\n"
append _dbgConfigHelp "    Enables recording DBG block GRC input. The user can create GRC input by\n"
append _dbgConfigHelp "    writing to DBG_REG_CPU_DEBUG_DATA. The GRC input always appear in the lowest\n"
append _dbgConfigHelp "    dword of a cycle, with HW ID 0. If omitted, GRC input is disabled.\n\n"

add_help dbgConfig $_dbgConfigHelp

# parse and config a filter / trigger state constraint
proc parseConstraint {constraintList} {
	if {![string is list $constraintList]} {
		error "ERROR: a constraint params list is expected"
	}
	set listLen [llength $constraintList]
	if {$listLen < 4 || $listLen > 8} {
		error "ERROR: a constraint must contain 4-8 elements"
	}
	set must [lindex $constraintList 0]
	set op [lindex $constraintList 1]
	set data [lindex $constraintList 2]
	set dataMask [lindex $constraintList 3]
	set dwordOffset 0
	set cycleOffset 0
	set compareFrame 0
	set frameBit 0
	if {$listLen >= 5} {
		set dwordOffset [lindex $constraintList 4]
	}
	if {$listLen >= 6} {
		set cycleOffset [lindex $constraintList 5]
	}
	if {$listLen >= 7} {
		set compareFrame [lindex $constraintList 6]
		if {$compareFrame} {
			if {$listLen != 8} {
				error "ERROR: a frame bit for comparison must be specified"
			}
			set frameBit [lindex $constraintList 7]
		}
	}
	dbgBusAddConstraint $op $data $dataMask $compareFrame $frameBit $cycleOffset $dwordOffset $must
}

# config the DBG block according to the specified args
# returns 1 if successful, 0 otherwise.

proc dbgConfig {args} {
	
	set errMsg ""

	catch {
		parseOptions $args
		_dbgConfigParsedOptions
	} errMsg
	
	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}

proc _dbgConfigParsedOptions {} {

	upvar optionsArr optionsArr
	
	if {[handleParseOption -help]} {
		help dbgConfig
		return
	}

	set oneShot [handleParseOption -oneShot]
	set grcEn [handleParseOption -grc]

	# reset the DBG block
	dbgBusReset $oneShot 0 0 $grcEn

	# check -pci option
	handleParseOption -pci 1 {0 1} {sizeInKb {
		dbgBusSetPciOutput $sizeInKb
		return 1
	}}

	# check -nw option
	handleParseOption -nw {1 4} {0 1} {args {
        set numArgs [llength $args]
        set portId [lindex $args 0]
        set destMacAddr "00:50:C2:2C:71:9C"
        set dataLimitSizeKB 0
		set otherEngineDevId 0
        set sendToOtherEngine 0
        if {$numArgs >= 2} {
            set destMacAddr [lindex $args 1]
        }
        if {$numArgs >= 3} {
            set dataLimitSizeKB [lindex $args 2]
        }
        if {$numArgs >= 4} {
            set otherEngineDevId [lindex $args 3]
            set sendToOtherEngine 1
        }
        set macBytes [split $destMacAddr :]
        if {[llength $macBytes] != 6} {
			error "ERROR: invalid destMacAddr ($destMacAddr)"
        }
        set macLo32 0x[lindex $macBytes 0][lindex $macBytes 1][lindex $macBytes 2][lindex $macBytes 3]
        set macHi16 0x[lindex $macBytes 4][lindex $macBytes 5]
        # config current device
		dbgBusSetNwOutput $portId $macLo32 $macHi16 $dataLimitSizeKB $sendToOtherEngine 0
        if {$sendToOtherEngine} {
            # config other device
            device $otherEngineDevId
	        dbgBusReset $oneShot $hwDwords $grcEn
		    dbgBusSetNwOutput $portId 0 0 0 0 1
        }
		return 1
    }}

	# check -block option
	handleParseOption -block {2 7} {0 Inf} {args {
            set numArgs [llength $args]
            set block [lindex $args 0]
            set line [lindex $args 1]
            set cycleEnable 0xf
		    set rightShift 0
		    set forceValid 0
		    set forceFrame 0
            if {$numArgs >= 3} {
                set cycleEnable [lindex $args 2]
            }
            if {$numArgs >= 4} {
                set rightShift [lindex $args 3]
				if {$rightShift > 3} {
				    puts "ERROR: rightShift must be 0-3"
					return 0
				}
			}
            if {$numArgs >= 5} {
                set forceValid [lindex $args 4]
            }
            if {$numArgs >= 6} {
                set forceFrame [lindex $args 5]
            }
    	dbgBusEnableBlock $block $line $cycleEnable $rightShift $forceValid $forceFrame
		return 1
	}}

	# check -timestamp option
	handleParseOption -timestamp {0 3} {0 1} {args {
		set numArgs [llength $args]
		set validEnable 0x7
		set frameEnable 0
		set tickLen 0
		if {$numArgs >= 1} {
			set validEnable [lindex $args 0]
		}
		if {$numArgs >= 2} {
			set frameEnable [lindex $args 1]
		}
		if {$numArgs >= 3 && [lindex $args 2] > 0} {
			set tickLen [expr [lindex $args 2] - 1]
		}
		dbgBusEnableTimestamp $validEnable $frameEnable $tickLen
		return 1
	}}

	# check -storm option
	handleParseOption -storm {1 2} {0 Inf} {args {
            set numArgs [llength $args]
		    if {$numArgs < 1 || $numArgs > 2} {
			    puts "ERROR: -storm should have 1-2 arguments"
			    return 0
		    }
            set storm [lindex $args 0]
            set mode rh
		    if {$numArgs >= 2} {
			    set mode [lindex $args 1]
		    }
		dbgBusEnableStorm $storm $mode
		return 1
	}}

	# check -rh option
	handleParseOption -rh 1 {0 1} {stormLetters {
        foreach letter [split $stormLetters ""] {
            dbgBusEnableStorm $letter rh
        }
		return 1
	}}

	# check -semFilter option
	handleParseOption -semFilter {2 5} {0 Inf} {args {
            set numArgs [llength $args]
		    if {$numArgs < 3} {
			    puts "ERROR: -semFilter should have at least 3 arguments"
			    return 0
		    }
            set stormLetter [lindex $args 0]
            set filterType [lindex $args 1]
		switch $filterType { 
		    eid
		    {
		            if {$numArgs != 5} {
			            puts "ERROR: EID SEM filter should have 5 arguments"
			            return 0
				}
                    set eidFilterType [lindex $args 2]
				switch $eidFilterType { 
					range
					{
                            dbgBusAddEidRangeSemFilter $stormLetter [lindex $args 3] [lindex $args 4]
					}
					mask
					{
                            dbgBusAddEidMaskSemFilter $stormLetter [lindex $args 3] [lindex $args 4]
					}
					default
					{
						error "ERROR: '$eidFilterType' is not a valid EID SEM filter type"
					}
				}
			}
			cid
			{
                    dbgBusAddCidSemFilter $stormLetter [lindex $args 2]
			}
			default
			{
				error "ERROR: '$filterType' is not a valid SEM filter type"
			}
		}
		return 1
	}}

	# check -filter option
	handleParseOption -filter {1 5} {0 1} {args {
		set numArgs [llength $args]
        set constMsgLen 1
        set argId 0
        if {[string is integer [lindex $args $argId]]} {
            set constMsgLen [lindex $args $argId]
            incr argId
        }
        dbgBusEnableFilter $constMsgLen
	    for {} {$argId < $numArgs} {incr argId} {
			parseConstraint [lindex $args $argId]
		}
		return 1
	}}

	# check -trigger option
	handleParseOption -trigger {1 9} {0 1} {args {
		set numArgs [llength $args]
		set recPre 0
		set preChunks 0
		set recPost 1
		set postCycles 0
		set filterPre 0
		set filterPost 1
		set firstStateArgId 0
		if {$numArgs >= 1 && [string is integer [lindex $args 0]]} {
			set recPre [lindex $args 0]
			set firstStateArgId 1
		}
		if {$numArgs >= 2 && [string is integer [lindex $args 1]]} {
			set preChunks [lindex $args 1]
			set firstStateArgId 2
		}
		if {$numArgs >= 3 && [string is integer [lindex $args 2]]} {
			set recPost [lindex $args 2]
			set firstStateArgId 3
		}
		if {$numArgs >= 4 && [string is integer [lindex $args 3]]} {
			set postCycles [lindex $args 3]
			set firstStateArgId 4
		}
		if {$numArgs >= 5 && [string is integer [lindex $args 4]]} {
			set filterPre [lindex $args 4]
			set firstStateArgId 5
		}
		if {$numArgs >= 6 && [string is integer [lindex $args 5]]} {
			set filterPost [lindex $args 5]
			set firstStateArgId 6
		}
        dbgBusEnableTrigger $recPre $preChunks $recPost $postCycles $filterPre $filterPost
	    for {set argId $firstStateArgId} {$argId < $numArgs} {incr argId} {
			set stateList [lindex $args $argId]
			if {![string is list $stateList]} {
				error "ERROR: a trigger state params list is expected"
			}
			set listLen [llength $stateList]
			puts "listLen=$listLen"
			if {$listLen < 1 || $listLen > 7} {
				error "ERROR: a trigger state must contain 1-7 elements"
			}
			set stormLetter ""
			set constMsgLen 1
			set countToNext 1
			set elementId 0
			if {[llength [lindex $stateList $elementId]] == 1 && ![string is integer [lindex $stateList $elementId]]} {
				set stormLetter [lindex $stateList $elementId]
				incr elementId
			}
			if {[string is integer [lindex $stateList $elementId]] && [string is integer [lindex $stateList [expr $elementId + 1]]]} {
				set constMsgLen [lindex $stateList $elementId]
				incr elementId
				set countToNext [lindex $stateList $elementId]
				incr elementId
			}
			dbgBusAddTriggerState "${stormLetter}sem" $constMsgLen $countToNext
			for {} {$elementId < $listLen} {incr elementId} {
				parseConstraint [lindex $stateList $elementId]
			}
		}
		return 1
	}}

    set invalidOptions [array names optionsArr]
    if {[llength $invalidOptions] > 0} {
        error "ERROR: invalid option: [lindex $invalidOptions 0]"
    }
}

# start Debug Bus recording
proc dbgStart {} {
	dbgBusStart
}

# stop Debug Bus recording
proc dbgStop {} {
	dbgBusStop
}

# Dump Debug Bus recording to a file
proc dbgDump { fileName } {
	dbgBusDump $fileName
}

# config the DBG block to record latency probes from multiple blocks
# returns 1 if successful, 0 otherwise.
proc dbgConfigLatencyProbes {} {

	set blocks {brb btb cau ccfc dorq igu mcm msem muld nig pbf pglue_b pcm prm \
            	prs psem pswhst2 pswhst pswrd2 pswrd pswwr ptu tcfc tcm tmld \
				tsem ucm usdm usem xcm xsdm xsem xyld ycm ysem}
	
    set argsList "-timestamp"
	
	foreach block $blocks {
		lappend argsList -block $block latency 0x7 3
	}
	
	return [dbgConfig {*}$argsList]
}


# config the DBG to record network traffic
# returns 1 if successful, 0 otherwise.

proc dbgConfigNwTrace {args} {

	set errMsg ""

	catch {		
		parseOptions $args
		
		if {[handleParseOption -help]} {
			help dbgConfigNwTrace
			return
		}
	
		set lb [handleParseOption -loopback]
		set tx [handleParseOption -tx]
		set rx [handleParseOption -rx]
	
		if { !($rx ^ $tx) } {
			error "ERROR: either -rx or -tx option must be specified, but not both"
		}
	
		set port 0
		handleParseOption -port 1 1 {x {
			upvar 2 port port
			set port $x
			return 1
		}}
	
		set lineOffset [expr $port*2 + $lb]
	
		if {$rx} {
			# line 20 == #line36 == wc_main_inp_data0
			lappend optionsArr(-block) [list brb [expr 20 + $lineOffset]]
		}
	
		if ($tx) {
			# line 8 == #line8a == pkt_rc_out_data0	
			lappend optionsArr(-block) [list btb [expr 8 + $lineOffset]]
		}
	
		_dbgConfigParsedOptions
	} errMsg
	
	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}

set _dbgConfigNwTraceHelp "Configures the debug block to record RX/TX traffic.\n"
append _dbgConfigNwTraceHelp "Usage: dbgConfigNwTrace <options> \n"
append _dbgConfigNwTraceHelp "where <options> is any of the following:\n\n"
append _dbgConfigNwTraceHelp "-rx or -tx\n"
append _dbgConfigNwTraceHelp "	specifies whether to record network RX or TX traffic. Required.\n"
append _dbgConfigNwTraceHelp "-port <portNum>\n" 
append _dbgConfigNwTraceHelp "	specifies the port number (0 or 1) to record from. Required.\n"
append _dbgConfigNwTraceHelp "-loopback\n"
append _dbgConfigNwTraceHelp "	Specifies whether to record the loopback port or real port. Optional.\n"
append _dbgConfigNwTraceHelp "Other options are the same as dbgConfig options, except the -block option\n"
append _dbgConfigNwTraceHelp "which is configured automatically. Use 'dbgConfig -help' to see all options.\n"

add_help dbgConfigNwTrace $_dbgConfigNwTraceHelp


########################################### Idle Check ##############################################

set _idleChkHelp    "Perform idle check.\n"
append _idleChkHelp "Usage: idleChk \[<fileName>\] \[<options>\]\n"
append _idleChkHelp "<fileName> - optional output text file.\n"
append _idleChkHelp "<options>  - any of the following:\n\n"
append _idleChkHelp "-nofwver\n"
append _idleChkHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _idleChkHelp "    Consequently, they may need to be specified manually on parsing.\n\n"
append _idleChkHelp "-via_mfw\n"
append _idleChkHelp "    If specified, perform secure idleChk via MFW mailbox request.\n"
append _idleChkHelp "-parse \[<data fileName>\]\n"
append _idleChkHelp "    If specified, parse idleChk.raw data into results text.\n"
append _idleChkHelp "    <data fileName> - optional idleChk raw data input file.\n"

add_help idleChk $_idleChkHelp

# Runs the Idle Check tool. returns 1 if successful, 0 otherwise.

proc idleChk { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help idleChk
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args
		
		set via_mfw 0
		set parse 0
		
		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}
		
		if {[handleParseOption -via_mfw]} {
			set via_mfw 1
		}
		
		if {[handleParseOption -parse {0 1} {0 1} {args {
			
			set numArgs [llength $args]			
	                if {$numArgs == 1} {
				upvar 2 parse_filename parse_filename
				set parse_filename [lindex $args 0]				
			}
			return 1
		}}]} {
			set parse 1			
		}		
				
		# If no filename specified
		if {$fileName == ""} {
			if {$via_mfw & $parse} {
				return [dbgIdleChk -via_mfw -parse]
			} elseif {$via_mfw} {
				return [dbgIdleChk -via_mfw]
			} elseif {$parse} {				
				if {[info exists parse_filename]} {
					return [dbgIdleChk -parse $parse_filename]
				} else {
					return [dbgIdleChk -parse]
				}
				
			} else {
				return [dbgIdleChk]
			}				
		} else {
			if {$via_mfw & $parse} {
				return [dbgIdleChk $fileName -via_mfw -parse]
			} elseif {$via_mfw} {
				return [dbgIdleChk $fileName -via_mfw]
			} elseif {$parse} {
				if {[info exists parse_filename]} {
					return [dbgIdleChk $fileName -parse $parse_filename]
				} else {
					return [dbgIdleChk $fileName -parse]
				}				
			} else {
				return [dbgIdleChk $fileName]
			}			
		}

	} errMsg

	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}

interp alias {} idleCheck {} idleChk

########################################### MCP Trace ################################################

set _mcpTraceHelp    "Dumps and parses the MCP Trace.\n"
append _mcpTraceHelp "Usage: mcpTrace \[<fileName>\] \[<options>\]\n"
append _mcpTraceHelp "<fileName> - optional output text file.\n"
append _mcpTraceHelp "<options>  - any of the following:\n\n"
append _mcpTraceHelp "-nomcp\n"
append _mcpTraceHelp "    If specified, no MCP commands are sent. Consequently, parities are not\n"
append _mcpTraceHelp "    masked, and MCP is not halted during MCP scratchpad dump.\n\n"
append _mcpTraceHelp "-nofwver\n"
append _mcpTraceHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _mcpTraceHelp "    Consequently, they may need to be specified manually on parsing.\n\n"

add_help mcpTrace $_mcpTraceHelp

# Runs the MCP Trace tool. returns 1 if successful, 0 otherwise.

proc mcpTrace { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help mcpTrace
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args

		# dbgGrcReset gives lower layers a chance to prepare for GRC Dump. An empty implementation is provided for layers that don't require this.
		dbgGrcReset

		# check -nomcp option
		if {[handleParseOption -nomcp]} {
			dbgGrcConfig nomcp 1
		}

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		if {$fileName == ""} {
			return [dbgMcpTrace]
		} else {
			return [dbgMcpTrace $fileName]
		}

	} errMsg

	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}


########################################### Reg FIFO ################################################

set _regFifoHelp    "Dumps and parses the contents of the GRC FIFO.\n"
append _regFifoHelp "Usage: regFifo \[<fileName>\] \[<options>\]\n"
append _regFifoHelp "<fileName> - optional output text file.\n"
append _regFifoHelp "<options>  - any of the following:\n\n"
append _regFifoHelp "-nofwver\n"
append _regFifoHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _regFifoHelp "    Consequently, they may need to be specified manually on parsing.\n\n"

add_help regFifo $_regFifoHelp

# Runs the regFifo tool. returns 1 if successful, 0 otherwise.

proc regFifo { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help regFifo
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		if {$fileName == ""} {
			return [dbgRegFifo]
		} else {
			return [dbgRegFifo $fileName]
		}

	} errMsg

	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}


########################################### IGU FIFO ################################################

set _iguFifoHelp    "Dumps and parses the contents of the IGU FIFO.\n"
append _iguFifoHelp "Usage: iguFifo \[<fileName>\] \[<options>\]\n"
append _iguFifoHelp "<fileName> - optional output text file.\n"
append _iguFifoHelp "<options>  - any of the following:\n\n"
append _iguFifoHelp "-nofwver\n"
append _iguFifoHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _iguFifoHelp "    Consequently, they may need to be specified manually on parsing.\n\n"

add_help iguFifo $_iguFifoHelp

# Runs the iguFifo tool. returns 1 if successful, 0 otherwise.

proc iguFifo { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help regFifo
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		if {$fileName == ""} {
			return [dbgIguFifo]
		} else {
			return [dbgIguFifo $fileName]
		}

	} errMsg

	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}


############################################ Protection Override ###############################################

set _protectionOverrideHelp    "Dumps and parses the contents of the protection override windows.\n"
append _protectionOverrideHelp "Usage: protectionOverride \[<fileName>\] \[<options>\]\n"
append _protectionOverrideHelp "<fileName> - optional output text file.\n"
append _protectionOverrideHelp "<options>  - any of the following:\n\n"
append _protectionOverrideHelp "-nofwver\n"
append _protectionOverrideHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _protectionOverrideHelp "    Consequently, they may need to be specified manually on parsing.\n\n"

add_help protectionOverride $_protectionOverrideHelp

# Runs the protectionOverride tool. returns 1 if successful, 0 otherwise.

proc protectionOverride { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help protectionOverride
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		if {$fileName == ""} {
			return [dbgProtectionOverride]
		} else {
			return [dbgProtectionOverride $fileName]
		}

	} errMsg

	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}


########################################### FW Asserts ################################################

set _fwAssertsHelp    "Dumps and parses the FW Asserts.\n"
append _fwAssertsHelp "Usage: fwAsserts \[<fileName>\] \[<options>\]\n"
append _fwAssertsHelp "<fileName> - optional output text file.\n"
append _fwAssertsHelp "<options>  - any of the following:\n\n"
append _fwAssertsHelp "-nofwver\n"
append _fwAssertsHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _fwAssertsHelp "    Consequently, they may need to be specified manually on parsing.\n\n"

add_help fwAsserts $_fwAssertsHelp

# Runs the fwAsserts tool. returns 1 if successful, 0 otherwise.

proc fwAsserts { args } {

    set errMsg ""

	catch {

		set fileName ""

		# check first argument (possibly a file name)
		if {[llength $args] > 0} {
			set firstArg [lindex $args 0]
			if {$firstArg == "-help"} {
				help fwAsserts
				return
			}
			if {![string match "-*" $firstArg]} {
				set fileName $firstArg
				set args [lrange $args 1 end]
			}
		}

		# parse options
		parseOptions $args
		
		set via_mfw 0
		set parse 0

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		if {[handleParseOption -via_mfw]} {
			set via_mfw 1
		}
		
		if {[handleParseOption -parse {0 1} {0 1} {args {
			
			set numArgs [llength $args]			
	                if {$numArgs == 1} {
				upvar 2 parse_filename parse_filename
				set parse_filename [lindex $args 0]				
			}
			return 1
		}}]} {
			set parse 1			
		}		
				
		# If no filename specified
		if {$fileName == ""} {
			if {$via_mfw & $parse} {
				return [dbgFwAsserts -via_mfw -parse]
			} elseif {$via_mfw} {
				return [dbgFwAsserts -via_mfw]
			} elseif {$parse} {				
				if {[info exists parse_filename]} {
					return [dbgFwAsserts -parse $parse_filename]
				} else {
					return [dbgFwAsserts -parse]
				}
				
			} else {
				return [dbgFwAsserts]
			}				
		} else {
			if {$via_mfw & $parse} {
				return [dbgFwAsserts $fileName -via_mfw -parse]
			} elseif {$via_mfw} {
				return [dbgFwAsserts $fileName -via_mfw]
			} elseif {$parse} {
				if {[info exists parse_filename]} {
					return [dbgFwAsserts $fileName -parse $parse_filename]
				} else {
					return [dbgFwAsserts $fileName -parse]
				}				
			} else {
				return [dbgFwAsserts $fileName]
			}			
		}

	} errMsg

	if {$errMsg != ""} {
		puts stderr $errMsg
		return 0
	}

	return 1
}


########################################## GRC Dump #################################################

set _grcDumpHelp    "Performs GRC Dump into a binary file.\n"
append _grcDumpHelp "By default, all memories are included in the dump, except IORs, VFC and\n"
append _grcDumpHelp "BRB/BTB/BMB big RAM. Memories can be included in or excluded from the dump,\n"
append _grcDumpHelp "overriding the default.\n"
append _grcDumpHelp "Supported memories: ram, pbuf, ior, vfc, ctx, pxp, rss, cau, cau_ext, qm,\n"
append _grcDumpHelp "    mcp, cfc, igu, brb, btb, bmb, nig, muld, prs, dmae, tm, sem, sdm, cm,\n"
append _grcDumpHelp "    dif, gfs, dorq, phy.\n"
append _grcDumpHelp "Use 'regs' as a memory name for registers dump.\n"
append _grcDumpHelp "Use 'mcp_hw_dump' as a memory name for MCP HW dump (dumped from NVRAM).\n"
append _grcDumpHelp "Use 'static' as a memory name for Static Debug dump.\n"
append _grcDumpHelp "If 'ior' or 'vfc' are included in the dump, the Storms are stalled before the\n"
append _grcDumpHelp "dump, and remain stalled after the dump. The Storms can be un-stalled after the\n"
append _grcDumpHelp "dump by specifying the -unstall option (see below).\n"
append _grcDumpHelp "Usage: grcDump <dirName> \[<options>\]\n"
append _grcDumpHelp "<dirName> - output folder\n"
append _grcDumpHelp "<options> - any of the following:\n\n"
append _grcDumpHelp "-exclude <memName1> <memName2> ...\n"
append _grcDumpHelp "    Excludes the specified memories from the dump.\n"
append _grcDumpHelp "    <memName#>: a memory name (e.g. -exclude ctx qm pxp).\n\n"
append _grcDumpHelp "-include <memName1> <memName2> ...\n"
append _grcDumpHelp "    Includes the specified memories in the dump.\n"
append _grcDumpHelp "    <memName#>: a memory name (e.g. -include ior vfc).\n\n"
append _grcDumpHelp "-only <memName1> <memName2> ...\n"
append _grcDumpHelp "    Includes only the specified memories in the dump.\n"
append _grcDumpHelp "    All other memories are excluded.\n"
append _grcDumpHelp "    <memName#>: a memory name (e.g. -only ctx ram).\n\n"
append _grcDumpHelp "-crash\n"
append _grcDumpHelp "    If specified, all memories are dumped, including those excluded by default.\n\n"
append _grcDumpHelp "-info\n"
append _grcDumpHelp "    If specified, only system info is dumped (all memories are excluded).\n\n"
append _grcDumpHelp "-storms <stormLetters>\n"
append _grcDumpHelp "    Includes the specified Storms in the dump. Other Storms are excluded.\n"
append _grcDumpHelp "    By default, all Storms are included.\n"
append _grcDumpHelp "    <stormLetters>: a string containing  x/y/p/t/m/u (e.g. -storms txu).\n\n"
append _grcDumpHelp "-unstall\n"
append _grcDumpHelp "    If specified, the Storms are un-stalled after the dump. Valid only if IORs\n"
append _grcDumpHelp "    or VFC are dumped, in which case the Storms are stalled automatically\n"
append _grcDumpHelp "    before the dump.\n\n"
append _grcDumpHelp "-nomcp\n"
append _grcDumpHelp "    If specified, no MCP commands are sent. Consequently, parities are not\n"
append _grcDumpHelp "    masked, and MCP is not halted during MCP scratchpad dump.\n\n"
append _grcDumpHelp "-nofwver\n"
append _grcDumpHelp "    If specified, the FW version and MFW version are not read from the chip.\n"
append _grcDumpHelp "    Consequently, they may need to be specified manually on parsing.\n\n"
append _grcDumpHelp "-parity_safe\n"
append _grcDumpHelp "    GRC Dump uses an MCP command to mask parities. If MCP is not available for\n"
append _grcDumpHelp "    some reason, GRC Dump will be performed without the MCP command, which can\n"
append _grcDumpHelp "    cause parity attentions. If -parity_safe is specified, GRC Dump will be\n"
append _grcDumpHelp "    performed only if MCP is available.\n\n"
append _grcDumpHelp "-no_idle_chk\n"
append _grcDumpHelp "    If specified, Idle Check is not performed.\n\n"
append _grcDumpHelp "-no_mcp_trace\n"
append _grcDumpHelp "    If specified, Mcp Trace is not performed.\n\n"

add_help grcDump $_grcDumpHelp

# empty implementation for GRC reset
proc dbgGrcReset {} {}

# performs GRC dump and stores the results in the specified dir
# returns 1 if successful, 0 otherwise.

proc grcDump { outDir args } {
    set errMsg ""

	catch {

		# parse options
		parseOptions $args

		if {$outDir == "-help"} {
			help grcDump
			return
		}

		if {[string match "-*" $outDir]} {
			error "ERROR: invalid output dir name"
		}

		# dbgGrcReset gives lower layers a chance to prepare for GRC Dump. An empty implementation is provided for layers that don't require this.
		dbgGrcReset

		# create output folder
		file mkdir $outDir

		set infoOnly 0
		if ([handleParseOption -info]) {
			set infoOnly 1
		}

		# run Idle Check
		if {![handleParseOption -no_idle_chk] && !$infoOnly} {
			puts "Dumping 2 Idle Checks..."
			idleChk {*}[concat ${outDir}/IdleChk1.txt $args]
			idleChk {*}[concat ${outDir}/IdleChk2.txt $args]
		}

		# run MCP Trace
		if {![handleParseOption -no_mcp_trace] && !$infoOnly} {
			puts "Dumping MCP Trace..."
			mcpTrace {*}[concat ${outDir}/McpTrace.txt $args]
		}

		puts "Dumping Reg FIFO..."
		regFifo {*}[concat ${outDir}/RegFifo.txt $args]

		puts "Dumping IGU FIFO..."
		iguFifo {*}[concat ${outDir}/IguFifo.txt $args]

		puts "Dumping Protection Override..."
		protectionOverride {*}[concat ${outDir}/ProtectionOverride.txt $args]

		# check -exclude option
		handleParseOption -exclude {0 Inf} {0 1} {args {
			foreach flag $args {
				dbgGrcConfig $flag 0
			}
			return 1
		}}

		# check -include option
		handleParseOption -include {0 Inf} {0 1} {args {
			foreach flag $args {
				dbgGrcConfig $flag 1
			}
			return 1
		}}

		# check -only option
		handleParseOption -only {0 Inf} {0 1} {args {
			dbgGrcConfig exclude_all 1
			foreach flag $args {
				dbgGrcConfig $flag 1
			}
			return 1
		}}

		# check -crash option
		if {[handleParseOption -crash]} {
			dbgGrcConfig crash 1
		}

		# check -info option
		if {$infoOnly} {
			dbgGrcConfig exclude_all 1
		}

		# check -parity_safe option
		if {[handleParseOption -parity_safe]} {
			dbgGrcConfig parity_safe 1
		}

		# check -storms option
		handleParseOption -storms 1 {0 1} {stormLetters {
			# disable all storms
			foreach letter { t m u x y p } {
				dbgGrcConfig ${letter}storm 0
			}
			# enable selected storms
			foreach letter [split $stormLetters ""] {
				dbgGrcConfig ${letter}storm 1
			}
			return 1
		}}

		# check -unstall option
		if {[handleParseOption -unstall]} {
			dbgGrcConfig unstall 1
		}

		# check -nomcp option
		if {[handleParseOption -nomcp]} {
			dbgGrcConfig nomcp 1
		}

		# check -nofwver option
		if {[handleParseOption -nofwver]} {
			dbgGrcConfig nofwver 1
		}

		# run GRC Dump
		puts "Starting GRC Dump..."
		dbgGrcDump ${outDir}/GrcDump.bin

	} errMsg
	
	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}

########################################## ILT Dump #################################################

set _iltDumpHelp    "Performs ILT Dump into a binary file.\n"
append _iltDumpHelp "By default, all ILT clients are included in the dump. Clients can be included\n"
append _iltDumpHelp "in or excluded from the dump, overriding the default.\n"
append _iltDumpHelp "Supported ILT clients: cduc, cdut.\n"
append _iltDumpHelp "Usage: iltDump <fileName> \[<options>\]\n"
append _iltDumpHelp "<fileName> - output binary dump file\n"
append _iltDumpHelp "<options> - any of the following:\n\n"
append _iltDumpHelp "-exclude <iltClient1> <iltClient2> ...\n"
append _iltDumpHelp "    Excludes the specified ILT clients from the dump.\n"
append _iltDumpHelp "    <iltClient#>: an ILT client name (e.g. -exclude cdut).\n\n"
append _iltDumpHelp "-include <iltClient1> <iltClient2> ...\n"
append _iltDumpHelp "    Includes the specified ILT clients in the dump.\n"
append _iltDumpHelp "    <iltClient#>: an ILT client name (e.g. -include cdut).\n\n"
append _iltDumpHelp "-only <iltClient1> <iltClient2> ...\n"
append _iltDumpHelp "    Includes only the specified ILT clients in the dump.\n"
append _iltDumpHelp "    All other ILT clients are excluded.\n"
append _iltDumpHelp "    <iltClient#>: an ILT client name (e.g. -only cdut).\n\n"

add_help iltDump $_iltDumpHelp

# performs ILT dump and stores the results in the specified output file
# returns 1 if successful, 0 otherwise.

proc iltDump { outFile args } {
    set errMsg ""

	catch {

		# parse options
		parseOptions $args

		if {$outFile == "-help"} {
			help iltDump
			return
		}

		if {[string match "-*" $outFile]} {
			error "ERROR: invalid output file name"
		}

		# check -exclude option
		handleParseOption -exclude {0 Inf} {0 1} {args {
			foreach flag $args {
				dbgGrcConfig $flag 0
			}
			return 1
		}}

		# check -include option
		handleParseOption -include {0 Inf} {0 1} {args {
			foreach flag $args {
				dbgGrcConfig $flag 1
			}
			return 1
		}}

		# check -only option
		handleParseOption -only {0 Inf} {0 1} {args {
			dbgGrcConfig exclude_all 1
			foreach flag $args {
				dbgGrcConfig $flag 1
			}
			return 1
		}}

		# run ILT Dump
		puts "Starting ILT Dump..."
		dbgIltDump $outFile

	} errMsg
	
	if {$errMsg != ""} {
        puts stderr $errMsg
		return 0
	}

	return 1
}
