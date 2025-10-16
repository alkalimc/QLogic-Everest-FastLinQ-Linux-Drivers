# NAMESPACE nvmcfgT
# Specify any dependency on other packages
package require Tcl

# Specify the package this module provides
package provide nvmcfgT 1.0

interp alias {} dump_all_cfg {}  nvm cfg -dump
interp alias {} dump_def_cfg {}  nvmcfgT::nvm_default_dump
interp alias {} swap_ports_cfg {} nvmcfgT::swap_ports_cfg
interp alias {} def_cfg {}    nvm cfg -def
interp alias {} set_driversim_mode {}  nvmcfgT::set_driversim_mode

namespace eval nvmcfgT {
# assuming meta data exists
# used for special announcements when a field is changed.
    variable PRINT
    set PRINT 1
    variable funcs
    variable ME_register_address 0x2a91fc
    variable CNIG_REG_NW_PORT_MODE 0x218200
    variable CNIG_REG_NIG_PORT0_CONF_K2 0x218200
    variable chip_num_reg   0xa408
    variable announcement
    variable is_default_dump false
    variable no_field_is_ignored false
    variable do_not_show_shared false
    variable do_not_show_per_port false
    variable do_not_show_per_function false
    variable no_group_name "<No Group>"
    variable is_swap_ports_cfg 0
    variable get_def_cfg 0
}

proc nvmcfgT::getChipNum {} {
        global ::current
     
        if {[info exist ::current(CHIP_NUM)]} {
            return $::current(CHIP_NUM)
        } else {
            return 0
        }
}

proc nvmcfgT::isVpdField {id} {
	variable is_vpd_string
	variable nvm_cfg
	if {[info exists nvm_cfg([set id],[set is_vpd_string])]} {
		return true
	} else {
		return false
	}
}


# getAllGroups: returns all the groups names
# limit_id_list is a subsset of the list that is relevant
proc nvmcfgT::getAllGroups {{limit_id_list {}} {always_get false}} {
	variable IDs
	variable no_group_name
        variable MNM_capability
        variable svid_val

	set result {}
        set mnm_groups {}

	if {[llength $limit_id_list] == 0} {
		set limit_id_list $IDs
	}
	foreach id $limit_id_list {
                
		if {!$always_get} {
			if {[isFieldIgnored $id]} {
				continue
			}
		}
		
                if {[isGroupDefined $id]} {
			lappend result [getGroup $id]
		}                
	}
        
	set groups [lsort -unique -dictionary $result]
                
        #Check if there are IDs with no group
	set no_grouped [getGroupIds $no_group_name $limit_id_list]
        if {[llength $no_grouped] > 0} {
		lappend groups $no_group_name
	}
	
        #Show Multi Mode groups at the end of the groups menu
        set mnm_groups_idx [lsearch -all $groups "Multi Mode*" ]
        if {[llength $mnm_groups_idx] > 0} {
            #find Multi Mode groups indexes in groups list
            foreach idx $mnm_groups_idx {
                lappend mnm_groups [lindex $groups $idx]
            }
            
            #remove Multi Mode groups from groups list
            foreach idx [lreverse $mnm_groups_idx] {
                set groups [lreplace $groups $idx $idx]
            }
           
            #Place Multi Mode groups at the end of groups list
            for {set idx 0 } {$idx < [llength $mnm_groups]} {incr idx} {
                lappend groups [lindex $mnm_groups $idx]
            }
        }

        return $groups
}

proc nvmcfgT::isPrefix {text sub_text} {
	set shorter_length [string length $sub_text]
	if {$shorter_length == 0} {
	#empty string do not count
		return false
	}
	if {[string length $text] < $shorter_length} {
		return false
	}
	set sub_str_text [string range $text 0 [expr  {$shorter_length - 1}]]
	if {[string compare $sub_str_text $sub_text] == 0} {
		return true
	} else {
		return false
	}
}

proc nvmcfgT::getGroupIds {group_name {limit_id_list {}} {show_ignored false}} {
	variable nvm_cfg
	variable IDs
        variable MNM_capability
        variable svid_val

	if {[llength $limit_id_list] == 0} {
		set limit_id_list $IDs
	}
	set result {}
	foreach id $limit_id_list {
		#is there such an ID
		if {[lsearch $IDs $id] < 0} {
			continue
		}
		if {[isFieldIgnored $id]} {
			if {!$show_ignored} {
				continue
			}
		}
		if {[string equal $group_name [getGroup $id]]} {
			lappend result $id
		}
	}
	
	return $result
}

proc nvmcfgT::getGroup {id} {
		variable nvm_cfg
		variable no_group_name
		if {[info exists nvm_cfg([set id],group_name)]} {
			return [set nvm_cfg([set id],group_name)]
		} else {
			return $no_group_name
		}
}

proc nvmcfgT::isGroupDefined {id} {
		variable nvm_cfg
		return [info exists nvm_cfg([set id],group_name)]
}


#Check if device is capable of requested speed
proc nvmcfgT::is_mnm_capable {{capability} {speed}} {
	variable IDs
        variable nvm_cfg
        variable mnm_modes
        
        #find speed in modes that are enabled
        for {set i 0} {$i < [array size mnm_modes]} {incr i} {
            set mode [expr {0x1 << $i}]
	        if {[expr {$capability & $mode}]} {
		if {[string first $speed $mnm_modes($mode)] != -1} {
                    return true
                }
            }
        }
       
        return false
}

proc nvmcfgT::isFieldIgnored {id} {
	variable no_field_is_ignored
	variable IDs
	variable nvm_cfg
        variable mnm_modes
        variable get_def_cfg
        variable MNM_capability
        variable svid_val
	       
        if {$no_field_is_ignored} {
		return false
	}

        #if id doesn't exist in IDs list, then ignore it
	if {[lsearch -exact $IDs $id] == -1} {
            return true
        }

        # If Multi-Network option is disabled, don't show MNM options
        if {[string match "MNM *" $nvm_cfg($id,name)]} {
            if { $MNM_capability == 0} {
                return true
            }
        }

        # If MNM not capable of 10G, don't show MNM 10G options
        if {[string match "MNM 10G *" $nvm_cfg($id,name)]} {
            if {![is_mnm_capable $MNM_capability 10G]}  {
                return true
            }
        }

        # If MNM not capable of 25G, don't show MNM 25G options
        if {[string match "MNM 25G *" $nvm_cfg($id,name)]} {
            if {![is_mnm_capable $MNM_capability 25G]}  {
                return true
            }
        }

        # If MNM not capable of 40G, don't show MNM 40G options
        if {[string match "MNM 40G *" $nvm_cfg($id,name)]} {
            if {![is_mnm_capable $MNM_capability 40G]}  {
                return true
            }
        }

        # If MNM not capable of 50G, don't show MNM 50G options
        if {[string match "MNM 50G *" $nvm_cfg($id,name)]} {
            if {![is_mnm_capable $MNM_capability 50G]}  {
                return true
            }
        }

        # If MNM not capable of 100G, don't show MNM 100G options
        if {[string match "MNM 100G *" $nvm_cfg($id,name)]} {
            if {![is_mnm_capable $MNM_capability 100G]}  {
                return true
            }
        }

        #Don't show options 210, 211 on boards other than OEM 0x1590
        if {[string match "Option Kit PN" $nvm_cfg($id,name)] ||
            [string match "Spare PN" $nvm_cfg($id,name)]} {
            
            set SVID_ID 78
            set svid_val [get_elements $SVID_ID]
            if {$svid_val != 0x1590} {
                return true
            }
        }
  
	return false
}


proc nvmcfgT::find_id_for_field {name} {
    variable IDs
    variable nvm_cfg
    
    foreach curr_id $IDs {
        if {[string match -nocase *$name* $nvm_cfg([set curr_id],name)] != 0} {
            return $curr_id
        }
    }
    return -1
}

proc nvmcfgT::find_ids_for_field {name} {
    variable IDs
    variable nvm_cfg
    set find_ids {}
    
    foreach curr_id $IDs {
        if {[string match -nocase *$name* $nvm_cfg([set curr_id],name)] != 0} {
            lappend find_ids $curr_id
        }
    }
    return $find_ids
}

proc nvmcfgT::find_boards_for_field {name} {
    variable boards
    set find_boards {}
   
    foreach curr_board $boards {
        if {[string match -nocase *$name* $curr_board] != 0} {
            lappend find_boards $curr_board
        }
    }
    return $find_boards
}


proc nvmcfgT::nvm_cfg_no_print {args} {
    variable PRINT
    set PRINT 0
    set ans [nvm_cfg $args]
    set PRINT 1
    return $ans
}

proc nvmcfgT::isGlobal {id} {
	variable nvm_cfg
        if {[string equal $nvm_cfg($id,entity_name) "glob"]} {
                return true
        } else {
                return false
        }	
}

proc nvmcfgT::isPerPath {id} {
	variable nvm_cfg
	if {[string equal $nvm_cfg($id,entity_name) "path"]} {
                return true
        } else {
                return false
        }
}

proc nvmcfgT::isPerPort {id} {
	variable nvm_cfg
	if {[string equal $nvm_cfg($id,entity_name) "port"]} {
                return true
        } else {
                return false
        }
}

proc nvmcfgT::isPerFunc {id} {
	variable nvm_cfg
	if {[string equal $nvm_cfg($id,entity_name) "func"]} {
                return true
        } else {
                return false
        }	
}


proc nvmcfgT::showPerFunctionFields {} {
	variable do_not_show_per_function
	variable is_default_dump
	if {$do_not_show_per_function} {
		return false
	} else {
		if {$is_default_dump} {
			return false
		} else {
			return true
		}
	}
}

proc nvmcfgT::showPerPortFields {} {
	variable do_not_show_per_port

	if {$do_not_show_per_port} {
		return false
	} else {
		return true
	}
}

proc nvmcfgT::showSharedFields {} {
	variable do_not_show_shared
	if {$do_not_show_shared} {
		return false
	} else {
		return true
	}
}

proc nvmcfgT::showField {id} {	
	variable MNM_capability
        variable svid_val

	if {[isFieldIgnored $id]} {
		return false
	}
	if {[isGlobal $id]} {
		return [showSharedFields]
	}
	if {[isPerFunc $id]} {
		return [showPerFunctionFields]
	}
	return [showPerPortFields]
}

proc nvmcfgT::GetDefaultMaxNumOfPorts {board} {
    variable portsPerBoard
    return $portsPerBoard($board)
}

proc nvmcfgT::GetMaxNumOfPorts {} {
   variable get_def_cfg
   
   set port_mode_opt_id [find_id_for_field {Network Port Mode}]

   set val [get_formatted_val $port_mode_opt_id false]
   
   # 0 DE_2x40G
   # 1 DE_2x50G
   # 2 DE_1x100G
   # 3 DE_4x10G_F
   # 4 DE_4x10G_E
   # 5 DE_4x20G
   # 11 DE_1x40G
   # 12 DE_2x25G
   # 13 DE_1x25G 
   # 14 DE_4x25G
   # 15 DE_2x10G
   switch $val {
       0     { return 2}
       1     { return 2}
       2     { return 1}
       3     { return 4}
       4     { return 4}
       5     { return 4}
       11    { return 1}
       12    { return 2}
       13    { return 1}
       14    { return 4}
       15    { return 2}
       default { return 2}
   }
}



# To read the current Path Num, simply read the absolute PF num (16 MSBs of ME Register)
# and then take the lsb of the result
proc nvmcfgT::GetCurrentPathNum {} {
        variable ME_register_address
    
        if {$::sys(NO_PCI) || ($::current(VIRTUAL_NVM_MODE) == 1)} {
                return [expr {$::current(DEV) & 1}]
        } else {
                return [expr {([reg read $ME_register_address] >> 16) & 1}]
	}            
}

proc nvmcfgT::isFuncZero {} {
    variable ::current
    variable ME_register_address

    if {$::current(FUNC)} {
        return false
    }

    if {$::sys(NO_PCI) || ($::current(VIRTUAL_NVM_MODE) == 1)} {
	return [expr {($::current(DEV) == 1) || ($::current(DEV) == 2)}]
	
    } else {
	set me_reg [reg read $ME_register_address]
	set abs_pf_num [expr {$me_reg >> 16}]
	if {$abs_pf_num == 0} {
	    return true
	} else {
	    return false
	}
    }
}

proc nvmcfgT::gotoFuncZero {} {
    variable ::current
    while {![isFuncZero]} {
        set curr_dev $::current(DEV)
        set next_to_try [expr {$curr_dev - 1}]
        #if cannot keep going to prev device
        if {[catch {device $next_to_try -no_display}]} {
            return
        }
        if {[isFuncZero]} {
            return
        }
    }
}

proc nvmcfgT::CreateString {char iter} {
	set result ""
	for {set i 0} {$i < $iter} {incr i} {
		append result $char
	}
	return $result
}

proc nvmcfgT::CreateHeadline {text {additional_first_char {}} {full_line true}} {
set half_length [expr [string length $text] / 2]
set num_spaces [expr {40 - $half_length}]
set result [CreateString " " $num_spaces]
append result "[set additional_first_char][set text]\n"

if {$full_line} {
	set num_spaces 0
	set num_lines [expr {80 - [string length $additional_first_char]}]
} else {
	set num_spaces [expr {40 - $half_length}]
	set num_lines [string length $text]
	
}
append result [set additional_first_char][CreateString " " $num_spaces]
append result [CreateString "-" $num_lines]
append result "\n"
return $result
}

proc nvmcfgT::GroupSelection {} {

	variable no_group_name
	set available_groups [getAllGroups]

	if {[llength $available_groups] == 0} {
		return $no_group_name
	}
	set selection [CreateHeadline "NVRAM Configuration Groups:" {} false]
	for {set i 1} {$i <= [llength $available_groups]} {incr i} {
		append selection "$i. [lindex $available_groups [expr {$i - 1}]]\n"
	}
	
	append selection "\n\nSelect: (q to quit)"
	
	while {1} {
		puts $selection
		set user_selection [gets stdin]
		if {[string equal $user_selection "q"] || [string equal $user_selection "Q"]} {
			return -1
		}
		if {[string equal $user_selection ""]} {
			return 0
		}
		if {![string is double $user_selection]} {
		# if not a number, maybe the group name

			set user_selection [GetGroupIndex $user_selection]

		} else {
		# menu is 1 to n+1 and not 0 to n
			set user_selection [expr {$user_selection - 1}]
		}

		if {($user_selection < 0) || ($user_selection >= [llength $available_groups])} {
			puts "Bad Choice."
			continue
		}
		
		return $user_selection
	}
}

proc nvmcfgT::DoesGroupNumberExist {number} {
	set available_groups [getAllGroups]
	if {($number < 0) || ($number >= [llength $available_groups])} {
		return false
	} else {
		return true
	}
}

proc nvmcfgT::GetGroupIndex {name} {
	set available_groups [getAllGroups]
	for {set i 0} {$i < [llength $available_groups]} {incr i} {
		if {[string equal [lindex $available_groups $i] $name]} {
			return $i
		}
	}
	return -1
}

proc nvmcfgT::GetNextGroupIndex {index} {
	incr index
	if {$index < 0} {
		return 0
	}
	set available_groups [getAllGroups]
	if {$index >= [llength $getAllGroups]} {
		return 0
	}
	return $index
}

# The main function,
# when args is empty work in interactive mode, which means:
# display menu, recieved commnads to do (<id>=<value>, cancel or save)
# otherwise, for every <id>=<value> set the value, for every <id>= return id s'value
# and if received a board name as the first and only argument it will set default value for the same board
proc nvmcfgT::nvm_cfg {args} {
    variable PRINT
    # When nvm_cfg has no args display menu
    variable IDs
    variable boards
    variable announcements
    variable nvm_config
    variable default_config
    variable mac_partition
    variable niv_config
    variable niv_profiles
    variable cfg_ext_shared_block
    variable nvm_cfg
    variable is_swap_ports_cfg
    variable mnm_modes
    variable get_def_cfg
    variable verify_cfg
    variable MNM_capability
    variable svid_val
    
    set announcements {}
    set batch [expr !$PRINT]
    set run true
    set save false
    set ans ""
    set dirty false
    set dev_idx -1
    set all_funcs 0
    set get_def_cfg 0
    set verify_cfg 0
    set is_special_nvm 0
    set special_nvm_func 0

    # ediag option -vcfg uses $::current(VERIFY_CFG) to verify configuration
    # according to .cfg input file
    if {$::current(VERIFY_CFG) == 1} {
        set verify_cfg 1
    }
    
         
    # read the whole NVM configuration
    set read_all 1
    set config_list [nvm_load 0 0 $read_all]
    set nvm_config [lindex $config_list 0]
    set default_config [lindex $config_list 1]

    # check MNM capability
    set MNM_CAPABILITY_ID 146
    if {[lsearch -exact $IDs $MNM_CAPABILITY_ID] == -1} {
        set MNM_capability 0
    } else {
	set MNM_capability [get_elements $MNM_CAPABILITY_ID]
    }
    
    # Get num of ports from nvm cfg opt 38
    set num_ports [GetMaxNumOfPorts]

    
    # Find out the groups names
    set group_list [nvmcfgT::getAllGroups]    
    set group_num [llength $group_list]
    set current_group_index 0
    if {$group_num > 1} {
	set enable_group_commands 1
    } else {
	set enable_group_commands 0
    }
    set first_run true
    set show_menu true
      
    while {$run} {
        
        if {[string equal $args {}]} {
                # interactive mode
		if {$enable_group_commands} {
			if {$first_run} {
				set current_group_index [GroupSelection]
				set first_run false
			}
			if {$current_group_index == -1} {
				set current_group_index 0
				set show_menu false
			}
			if {$show_menu} {
				display_menu false [lindex $group_list $current_group_index]	
			} else {
				display_menu false false true
			}
		} else {
			if {$show_menu} {
				display_menu
			} else {
				display_menu false false true
			}
		}
		if {$show_menu} {
                        set current_group_index [expr {($current_group_index + 1) % $group_num}]
		} else {
			set show_menu true
		}
                set commands [gets stdin]
                if {[string first "-all" $commands] != -1} {
                     set all_funcs 1
                     # remove -all flag
                     set commands [string replace $commands 0 3]
		}		
        } else {
		set first_run false
		set batch 1
		set save true
		set run false
		set commands $args
                set all_funcs 0
                set get_def_cfg   0
                set find_idx 0
		set is_special_nvm 0
		set special_nvm_func 0
                
                if {[string first "-all" $commands] != -1} {
                        set all_funcs 1
                        # remove -all flag
                        set commands [string replace $commands 0 3]
                        			
                } elseif {[string first "-func" $commands] != -1} {
                     set is_special_nvm 1
		     set special_nvm_func [lindex [string replace $commands 0 5] 0]
                     # remove -func flag
                     set commands [string replace $commands 0 7]
		     
		} elseif {[string first "-find" $commands] != -1} {
                    # nvm cfg -find <string>
                    # show all matching options with <string>
                    set name [string replace $commands 0 5]
                    if {[llength $name] > 0} {
                        set found_ids [find_ids_for_field $name]
                        if {[llength $found_ids] != 0} {
                            for {set i 0} {$i < [llength $found_ids]} {incr i} {
                                set idx [lindex $found_ids $i]
                                if {[does_id_exist_for_chip $idx [getChipNum]] &&
                                    ![isFieldIgnored $idx]} {
                                    ShowGroupInfo $idx $idx
                                }
                            }
                            return
                        } else {
                            #show all matching boards with <string>
                            set found_boards [find_boards_for_field $name]
                            if {[llength $found_boards] != 0} {
                                puts "$found_boards"
                                return
                            } else {
                                puts "Didn't find id or board for this string"
                                return
                            }
                        }
                    } else {
                        puts "usage: nvm cfg -find <string>"
                        return
                    }
                } elseif {[string first "-def" $commands] != -1} {
                        set get_def_cfg 1
                        # remove -def flag
                        set commands [string replace $commands 0 3]	

                } elseif {[string first "-verify" $commands] != -1} {
                        set verify_cfg 1
                        # remove -verify_cfg flag
                        set commands [string replace $commands 0 7]	
                }
        }
        
        for {set i 0} {$i<[llength $commands]} {incr i} {
                # Handle command line "nvm cfg id=data" or "nvm cfg id="
                if {[regexp {^(\d+)=(.*)$} [lindex $commands $i] id id data]} {
                    
			if {[isFieldIgnored $id]} {
				puts "Field [set id] is ignored."
                                 # in manufacture mode this error will cause ediag to exit, to stop production !!
                                 if {!$verify_cfg && ($::sys(SYSOP_MODE)==1)} {
                                     puts "ERROR: Setting value to id $id data $data failed"
                                     exit
                                 } else {
                                     continue
                                 }
                        } elseif {![does_id_exist_for_chip $id [getChipNum]]} {
                                puts "Field [set id] doesn't exist for this chip."
                                 # in manufacture mode this error will cause ediag to exit, to stop production !!
                                 if {!$verify_cfg && ($::sys(SYSOP_MODE)==1)} {
                                     puts "ERROR: Setting value to id $id data $data failed"
                                     exit
                                 } else {
                                     continue
                                 }
                        } else {
				if {![string equal $data {}]} {
                                        # Handle request "nvm cfg id=data"
                                        if {$get_def_cfg == 1} {
                                            puts "Setting value to DEF_CFG is not allowed !"
                                            return                                          
                                        }

					if {$all_funcs==1} {
						#update all devices option id=data
						for {set func_num 0} {$func_num < 16} {incr func_num} {
							set ::current(IS_SPECIAL_NVM_MODE) 1
							set ::current(SPECIAL_NVM_MODE_NUM_PORTS) [GetMaxNumOfPorts]
							set ::current(SPECIAL_NVM_MODE_FUNC_NUM) $func_num
							set res [set_feature_val $id $data]
        						set ::current(IS_SPECIAL_NVM_MODE) 0
						}
					} elseif {$is_special_nvm==1} {
						#update the requested nvm func
						set ::current(IS_SPECIAL_NVM_MODE) 1
						set ::current(SPECIAL_NVM_MODE_NUM_PORTS) [GetMaxNumOfPorts]
						set ::current(SPECIAL_NVM_MODE_FUNC_NUM) $special_nvm_func
						puts "Configure nvm_func $special_nvm_func option $id with value $data"
						set res [set_feature_val $id $data]
        					set ::current(IS_SPECIAL_NVM_MODE) 0			
					} else {
						#Update current device option id=data
						set res [set_feature_val $id $data]
					}
					if {!$dirty} {
						set dirty $res
					}
                                        if {($res == false) && !$verify_cfg} {

                                            # in manufacture mode this error will cause ediag to exit, to stop production !!
                                            if {$::sys(SYSOP_MODE)==1} {
                                                puts "ERROR: Setting value to id $id data $data failed"
                                                exit
                                            }
                                        }
				} else {
                                        # Handle request "nvm cfg id="
					if {$all_funcs==1} {
						#update all devices option id=data
						for {set func_num 0} {$func_num < 16} {incr func_num} {
							set ::current(IS_SPECIAL_NVM_MODE) 1
							set ::current(SPECIAL_NVM_MODE_NUM_PORTS) [GetMaxNumOfPorts]
							set ::current(SPECIAL_NVM_MODE_FUNC_NUM) $func_num
							set value [get_formatted_val $id false]
							if {![string equal $value false]} {
								lappend ans "func $func_num: $value"
							}
        						set ::current(IS_SPECIAL_NVM_MODE) 0
						}
					} elseif {$is_special_nvm==1} {
						#update the requested nvm func
						set ::current(IS_SPECIAL_NVM_MODE) 1
						set ::current(SPECIAL_NVM_MODE_NUM_PORTS) [GetMaxNumOfPorts]
						set ::current(SPECIAL_NVM_MODE_FUNC_NUM) $special_nvm_func
						puts "Read configuration from nvm_func $special_nvm_func"
						set value [get_formatted_val $id false]
						if {![string equal $value false]} {
							lappend ans $value
						}
						set ::current(IS_SPECIAL_NVM_MODE) 0
					} else {			
						set value [get_formatted_val $id false]
						if {![string equal $value false]} {
							lappend ans $value
						}
			      		}
				}
			}

                } elseif {[string equal [lindex $commands 0] -dump]} {
                        #Handle "nvm cfg -dump" reuqest
                        if {[string equal $args {}] || [llength $commands]>2} {
                            puts "You can only use dump filename without the menu and as only argument"
                            puts "Ignoring the dump request"
                            continue
                        }
                        set filename [lindex $args 1]
                        if {[string equal $filename ""]} {
                            error "When using -dump you must give a filename."                    
                        }
        
                        #mark that we are in special NVM mode (to enable getting/setting nvm values of all 8 functions, even if we are in SF mode
                        set ::current(IS_SPECIAL_NVM_MODE) 1
                        set ::current(SPECIAL_NVM_MODE_NUM_PORTS) $num_ports
                        dump_menu $filename
        
                        #disable special NVM mode
                        set ::current(IS_SPECIAL_NVM_MODE) 0               
                        return

		} elseif {[lsearch -exact $boards [lindex $commands $i]]!=-1} {
                        #Handle board setting "nvm cfg <board name>"
                        if {[string equal $args {}] || [llength $commands]>1} {
                            puts "You can only set default values without the menu and as only argument"
                            puts "Ignoring the default request"
                            continue
                        }
                        return [set_board_default_vals [lindex $commands $i]]
		
                } elseif {[isPrefix save [lindex $commands $i]]} {
                    set save true
                    set run false                
                    break
                } elseif {[isPrefix cancel [lindex $commands $i]]} {
                    set run false                
                    break
                } elseif { $enable_group_commands} {
			if {[isPrefix "list-groups" [lindex $commands $i]]} {
					
				set tmp_group_selection [GroupSelection]
				if {$tmp_group_selection == -1} {
					set show_menu false
				} else {
					set current_group_index $tmp_group_selection
				}
				break
						
			} elseif {[isPrefix group [lindex $commands $i]]} {
				incr i
				if {[llength $commands] < 2} {
					puts "After group, you must enter a group name or number. \n You can see the full list of groups by using the list-groups command."
					set show_menu false
					set i [llength $commands]
					break
				}
				set length_first_token [expr {[string length [lindex $commands 0]] + 1}]
				set group_name [string range $commands $length_first_token [string length $commands]]
				if {![string is integer $group_name]} {
					set current_group_index [GetGroupIndex $group_name]
				} else {
					#groups are actually 0-n and not 1-n+1
					set current_group_index [expr {$group_name - 1}]
				}
				if {![DoesGroupNumberExist $current_group_index]} {
					puts "No such group: [lindex $commands $i]\n You can see the full list of groups by using the list-groups command."
					set show_menu false
					set current_group_index 0
				}
				set i [llength $commands]
						
			} else {
                                #Handle "nvm cfg id-" request
				set split_string [split [lindex $commands $i] -]
				if {[llength $split_string] == 2} {
					set id_info_first [lindex $split_string 0]
					set id_info_second [lindex $split_string 1]
					if {((![string is integer $id_info_first]) || ((![string is integer $id_info_second]) && !($id_info_second == {})))} {
						puts "unknown argument [lindex $commands $i] : you can use <id>- or <id>-<id>."
						set show_menu false
					} else {
						if {$id_info_second == {}} {
							set id_info_second $id_info_first
						}
						set show_menu false
						ShowGroupInfo $id_info_first $id_info_second
						incr i
					}
				} else {
			                puts "unknown argument - [lindex $commands $i], ignored."
					puts "The allowed arguments are <id>=, <id>=<value> or <board type>"							
					set show_menu false
                                }
                        }
			
		} else {
                        puts "unknown argument - [lindex $commands $i], ignored."
                        puts "The allowed arguments are <id>=, <id>=<value> or <board type>"                
                }
        }
    }
    
    if {$save && $dirty} {
        if {!$batch} {
            puts "Config saved."
        }
        if {![string equal $announcements {}] &&
            ! $is_swap_ports_cfg} {
            puts -nonewline $announcements
        }

        # Set the current device configuration
        set write_all 0
	nvm_store $nvm_config $dev_idx $num_ports $write_all
    } else {
        if {!$batch} {
            puts "Config not saved."
        }        
        nvm_free_structs $nvm_config $default_config 
    }

    set announcements {}
    if {![string equal $args {}]} {
        return $ans
    }

    return
}


#Show the related group informationo for the first-last IDs
proc nvmcfgT::ShowGroupInfo {first last} {
	variable IDs
        variable svid_val
                
	if {$first > $last} {
		set temp $first
		set first $last
		set last $temp
	}
	set limiting_list {}
	set not_exists_text ""
	for {} {$first <= $last} {incr first} {
		if {[lsearch $IDs $first] > -1} {
			lappend limiting_list $first
		} else {
			append not_exists_text "ID $first Does not exist.\n"
		}
	}
	
	
	set result ""
	if {[llength $limiting_list] > 0} {
                
		set relevant_groups [getAllGroups $limiting_list true]                
		foreach group $relevant_groups {
			
			set relevant_ids [getGroupIds $group $limiting_list true]
			foreach id $relevant_ids {
				if {[isFieldIgnored $id]} {
					append result "ID $id Does not exist.\n"
				} else {
                                        append result [CreateHeadline "Group: $group (Group [expr {[GetGroupIndex $group] + 1}])"]
					append result [get_feature_line $id true]
                                        append result "\n\n"
				}
			}
			
		}
	}
	append result $not_exists_text
	catch {puts $result}

}


proc nvmcfgT::nvm_default_dump {output board} {
    variable default_dump_params
    variable is_default_dump
	variable no_field_is_ignored
	set old_no_field_ignored_value $no_field_is_ignored
	set no_field_is_ignored true
    array set default_dump_params {}
    set chip [get_default_device_id $board]
    if {$chip == -1} {
        puts "Error: Cannot determine DeviceID for Board $board"
        return
    }

    set total_dev [get_default_NumPorts $board]
    if {$total_dev == -1} {
        puts "Error: Cannot determine Num Ports for Board $board"
        return
    }
    set funcs $total_dev
    set default_dump_params(BOARD) $board
    # TBD: find chip number of this board
    set default_dump_params(CHIP_NUM) "57980"
    set default_dump_params(TOTAL_DEV) $total_dev
    set default_dump_params(FUNCS) $funcs
    set is_default_dump true
    dump_menu $output true default_dump_params
    set is_default_dump false
    set no_field_is_ignored $old_no_field_ignored_value
    return
}

# Check if this nvm cfg option ID should be excluded
# on the current device according to it's chip number
proc nvmcfgT::isFieldExcluded {id {chip_num -1}} {
	variable nvm_cfg
	if {$chip_num == -1} {
		set chip_num [getChipNum]
	}
        
        #check if this ID should be excluded on this chip
	if {[info exists nvm_cfg($id,exChipNumAttrs)]} {
		if {([lsearch $nvm_cfg($id,exChipNumAttrs) $chip_num]!=-1)} {
			return 1;
		}

		for {set i 0} {$i<[llength $nvm_cfg($id,exChipNumAttrs)]} {incr i} {	   
			set tcl_var nvmcfg_ignore_[lindex $nvm_cfg($id,exChipNumAttrs) $i]
			if {([info exists ::current(nvmcfg_ignore_[lindex $nvm_cfg($id,exChipNumAttrs) $i])])} {
				if {$::current(nvmcfg_ignore_[lindex $nvm_cfg($id,exChipNumAttrs) $i]) == 1} {
					return 1;
				}
			}
		}
	}

	return 0;
}

# check the defaults for a specic board for requested ids
# first look for the default in that board's array (id as key)
# otherwise checks if the value exists per port (id,port as key)
# otherwise checks if the value exists in the default's array
# otherwise checks if the value exists per port in the default's array (id,port as key)
# Returned Result: 0 - for success, otherwise, last ID with error
proc nvmcfgT::nvm_chk {board {fix false}} {


    variable nvm_cfg
    variable portMax
    variable funcs
    variable chkIDs
    variable boards
    variable nvm_config
    variable chipnumPerBoard
    variable IDs
    variable MNM_capability
    variable svid_val

    set errorStr {}
    set returned_result 0
    if {[lsearch -exact $boards $board]==-1} {
        error "Unknown board to check."
    }
    upvar #0 nvmcfgT::$board boardD
    upvar #0 nvmcfgT::default defaultD
    
    set curr_device $::current(DEV)
    if {[string equal $fix -fix]} {
        set fix true
    } elseif {![string equal $fix false]} {
        puts "unknown argument: $fix"
    }
  
    # setting the port to be port0
    gotoFuncZero
    set func0 $::current(DEV)
    set dirty false
    puts "Start checking from device $func0"
    
    set chipNum [getChipNum]
    if {$chipNum != $chipnumPerBoard($board)} {
        puts "ERROR: Request board is for $chipnumPerBoard($board), not for this chip ($chipNum) !"
        return
    }
    
    # read the whole NVM configuration
    set read_all 1
    set config_list [nvm_load 0 0 $read_all]
    set nvm_config [lindex $config_list 0]

    # check MNM capability
    set MNM_CAPABILITY_ID 146
    if {[lsearch -exact $IDs $MNM_CAPABILITY_ID] == -1} {
        set MNM_capability 0
    } else {
	set MNM_capability [get_elements $MNM_CAPABILITY_ID]
    }
    
    set portMax [GetDefaultMaxNumOfPorts $board]
    set num_vfs_idx [find_id_for_field {Number of VFs per PF}]

    for {set func_num 0} {$func_num < 16} {incr func_num} {
       
        set dev_idx [expr {$func_num + 1}]
        set port_num [expr {$func_num % $portMax}]
        for {set i 0} {$i<[llength $chkIDs]} {incr i} {
		set id [lindex $chkIDs $i]
		if {[isFieldIgnored $id]} {
			continue
		}
		
		set defVal {}
		regexp {^(\d+)} $id idNoV idNoV
		if {[isFieldExcluded $idNoV]} {
		    continue
		}
	    
	        # Global parameters should be checked
		if {[isGlobal $idNoV] && ($func_num > 0)} {
		    continue
		}
	    
		# Per-port parameters should be checked once per port
		if {([isPerPort $id] && ($func_num >= $portMax)) } {
			continue
		}
	    
		set no_default false
		set defVal [verify_default_value $id $board $port_num $func_num no_default]
		 # CQ79152 Avoid total num of VFs > 240 by setting 
		 # num Vfs 8 instead of 16 on two last functions.
		if {($id == $num_vfs_idx) && ($defVal == 16)} {
			if {($func_num == 14) || ($func_num == 15)} {
				set defVal 8
			}
		}

		if {$no_default} {
			error "couldn't find a value to verify for id $id , port $port_num"
		}

		if {[string equal $id $idNoV]} {
			set formattedDefVal [format_values $id [lreverse [checkUserInput $id $defVal]]]
					    
                        # Read value of this id in NVM in the loaction of func_num
			if {![string equal [get_formatted_val $id true $dev_idx $portMax] $formattedDefVal]} {
				if {[isGlobal $id]} {
					append errorStr "ID $id: \"$nvm_cfg($id,name)\" mismatch:\n"
				} elseif {[isPerPort $id] } {
					append errorStr "ID $id for port $port_num: \"$nvm_cfg($id,name)\" mismatch:\n"
				} else {
					append errorStr "ID $id for func $func_num: \"$nvm_cfg($id,name)\" mismatch:\n"
				}
				set returned_result $id
				append errorStr "   Expected $formattedDefVal\n"
				append errorStr "   Existing [get_formatted_val $id true $dev_idx $portMax]\n"
			}
			if {$fix} {
			    set_feature_val $id $defVal
			    set dirty true
			}
		} else {
		    if {[string first $defVal [get_formatted_val $idNoV true $dev_idx $portMax]]!=0} {
		        if {[isGlobal $idNoV]} {
		            append errorStr "ID $idNoV: \"$nvm_cfg($idNoV,name)\" mismatch\n"
		        } else {
		            append errorStr "ID $idNoV for port $port_num: \"$nvm_cfg($idNoV,name)\" mismatch\n"
		        }
		        set returned_result $id
		        append errorStr "   Expected prefix $defVal\n"
		        append errorStr "   Exsiting prefix [string range [get_formatted_val $idNoV true $dev_idx $portMax] 0 [expr [string length $defVal]-1]]\n"
		    }
		}
        }
    }

    if {$fix && $dirty} {
           set write_all 1
           nvm_store $nvm_config 0 0 $write_all	
    }
    
    if {[string equal $errorStr {}]} {
        puts "Nvm chk hasn't found any errors in the configuration"
		return $returned_result
    } else {
        puts "The following errors were found in the configuration:\n$errorStr"
		return $returned_result
    }
}

# this sub routine displays the menu
# it stops upon recieving 'q'
proc nvmcfgT::display_menu {{show_all true} {group_id false} {show_only_options false}} {
    variable IDs
    variable MNM_capability
    variable svid_val

    set lineCount 0
	set groups_list [getAllGroups]
	set group_num [llength $groups_list]
	if {!$show_only_options} {
		if {$show_all} {
			set list_of_IDs $IDs
		} else {
			set list_of_IDs [getGroupIds $group_id]
			if {[llength $list_of_IDs] == 0} {
				puts "Bad Group $group_id"
			}
			append	menuD [CreateHeadline "Group: $group_id (Group [expr {[GetGroupIndex $group_id] + 1}])"]
			
			set lineCount 2
		}
	
	
	
		for {set i 0} {$i<[llength $list_of_IDs]} {incr i} {
			if {[isFieldIgnored [lindex $list_of_IDs $i]]} {
				continue
			}
			set next_item [get_feature_line [lindex $list_of_IDs $i]]
			set next_lCount [regexp -all {\n} $next_item]
			if {$next_lCount>=22} {
				error "Item [lindex $list_of_IDs $i] is too big to fit in one screen."
			} elseif {[expr {$lineCount+$next_lCount}]<22} {
				append menuD $next_item
				incr lineCount $next_lCount
			} else {
				append menuD "more.., 'q' to quit"
				puts -nonewline $menuD
				set menuD $next_item
				set lineCount $next_lCount
				set ch [getc]
				puts {}
				if {[string equal $ch "q"]} {
					set menuD {}
					break
				}
			}
		}
	}
    append menuD { Choice (<option>=<value>, save, cancel}
	if {$group_num > 1} {
		append menuD {, group <group name>, list-groups, <ID>-<optional ID>)}
	} else {
		append menuD {)}
	}
	
    puts $menuD

}

proc nvmcfgT::dump_menu {output {defaultdump false} {defdumpparams none}} {
    variable IDs
    variable funcs
    upvar $defdumpparams def_params
    
    #remove redirect of puts to log file, if exist
    stdout on
          
    #dump nvm cfg to output file
    set outFile [open $output w]

    if {$defaultdump} {
	set board_name [set def_params(BOARD)]
    } else {
        set board_name "<board name>"
    }

    set manuf_ver_idx [find_id_for_field {Manufacture kit version}]

    puts $outFile "#######################################################"
    puts $outFile "# DO NOT EDIT!!! THIS FILE IS AUTOMATICALLY GENERATED."
    puts $outFile "#"
    puts $outFile "#Copyright(c) 2014-2017 Cavium, Inc., all rights reserved"
    puts $outFile "#Proprietary and Confidential Information."
    puts $outFile "#"
    puts $outFile "#"
   #puts $outFile "#Creation Date: [clock format [clock seconds] -format {%b. %d, %Y %I:%M:%S %p}]"
    
    puts $outFile "#Description: This file contains the NVRAM configuration for $board_name"
    puts $outFile "#             To incorporate these settings into the board's NVRAM, simply"
    puts $outFile "#             source this file in Ediag as follows"
    puts $outFile "#                 source <config_file>"
    puts $outFile "#"
    puts $outFile "#Design Name: $board_name"
    puts $outFile "#Diagnostic Tool Version: [version]"
    puts $outFile "#Software Release:"
    puts $outFile "#Customer Name:"
    puts $outFile "#Revision History:"
    puts $outFile "#"
    puts $outFile "#Manufacture kit version: [get_formatted_val $manuf_ver_idx]"
    puts $outFile "#Additional Comments:"
    puts $outFile "\n\n\n"
    if {$defaultdump} {
        set curr_device 1
        set total_dev $def_params(TOTAL_DEV)
        set funcs $def_params(FUNCS)
        } else {
        set curr_device $::current(DEV)
        set total_dev $::current(TOTAL_DEV)
        set funcs 16
    }

    

    for {set device_base $curr_device} {$device_base <= $total_dev} {incr device_base $funcs} {
        if {!$defaultdump} {
            device $device_base -no_display
        }
        set shared {}
        set perPorts {}
        set perDevMenu {}

	set perFunctions {}
        
        if {[catch {print_configuration $device_base shared perPorts perFunctions perDevMenu $defaultdump def_params} errMsg]} {
            close $outFile
            error $errMsg
        }

        for {set i 0} {$i<[llength $perDevMenu]} {incr i} {
            puts $outFile "################\n### Device [expr $i+$device_base] ###\n################\n"
            puts $outFile [lindex $perDevMenu $i]
            puts $outFile "\n\n"
        }

        set num_ports [GetMaxNumOfPorts]

        puts $outFile "##############\n### Shared ###\n##############\n"
        puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {} else {set ::current(IS_SPECIAL_NVM_MODE) 1}"  
        puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {} else {set ::current(SPECIAL_NVM_MODE_NUM_PORTS) $num_ports}"
        puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {device [expr {$device_base-$curr_device+1}]} else {set ::current(SPECIAL_NVM_MODE_FUNC_NUM) 0}"        
        puts $outFile "nvm cfg \\"
        puts $outFile $shared
        puts $outFile "\n\n"
	set max_count [llength $perFunctions]
	if {[llength $perPorts] > $max_count} {
		set max_count [llength $perPorts]
	}

        for {set i 0} {$i<[llength $perFunctions]} {incr i} {
            puts $outFile "################\n### Device [expr $i+$device_base] ###\n################\n"
            
            if {$i} {
                 puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {device [expr {$i+$device_base-$curr_device+1}]} else {set ::current(SPECIAL_NVM_MODE_FUNC_NUM) [expr $i]}"                
                 puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {} else {set ::current(SPECIAL_NVM_MODE_NUM_PORTS) $num_ports}" 
            }
            puts $outFile "nvm cfg \\"
	    set curr_line ""
	    if {$i < [llength $perPorts] } {
		set curr_line [lindex $perPorts $i]
	    }

	    if {$i < [llength $perFunctions] } {
		set curr_line "[set curr_line][lindex $perFunctions $i]"
	    }
	    puts $outFile $curr_line
            puts $outFile "\n\n"
        }
    }
    if {!$defaultdump} {
        device $curr_device -no_display
    }

    puts $outFile "if {\[catch {set x \$::current(IS_SPECIAL_NVM_MODE)}\]} {} else {set ::current(IS_SPECIAL_NVM_MODE) 0}"
    puts $outFile "device 1"
    close $outFile

}

proc nvmcfgT::initGroupDisplay {} {
	variable group_display_info
	if {[info exists group_display_info]} {
		array unset group_display_info
	}
	
	array set group_display_info {}
	set group_display_info(groups) {}

}

proc nvmcfgT::addIdInfo {id data} {
	variable group_display_info
	if {$data == {}} {
		return
	} else {
		set group_name [getGroup $id]
		if {[lsearch $group_display_info(groups) $group_name] == -1} {
			lappend group_display_info(groups) $group_name
		}
	}
	append group_display_info($group_name) $data
}

proc nvmcfgT::printGroupDisplay {} {
		variable group_display_info
		set groups_list $group_display_info(groups)
		if {[llength $groups_list] == 0} {
			return {}
		}
		
		foreach group_name $groups_list {
			append result [CreateHeadline "Group: $group_name (Group [expr {[GetGroupIndex $group_name] + 1}])"]
			append result $group_display_info($group_name)
			append result "\n\n"
		}
		return $result
}

## Implements nvm cfg -dump
proc nvmcfgT::print_configuration {dev sharedVar perPortsVar perFunctionVar perDevMenuVar {def_dump false} {def_params_var none}} {
    upvar $sharedVar shared
    upvar $perPortsVar perPorts
    upvar $perFunctionVar perFunction
    upvar $perDevMenuVar perDevMenu
    upvar $def_params_var def_params

    variable do_not_show_shared
    variable do_not_show_per_port
    variable do_not_show_per_function

    set old_show_shared $do_not_show_shared
    set old_show_per_port $do_not_show_per_port
    set old_show_per_function $do_not_show_per_function


    variable IDs
    variable nvm_cfg
    variable portMax
    variable funcs
    variable noDefault
    variable ::current
    variable nvm_config
    variable default_config
    variable mac_partition
    variable niv_config
    variable niv_profiles
    variable cfg_ext_shared_block
    variable MNM_capability
    variable svid_val
    
    # setting the port to be port0
    if {!$def_dump} {
        set cu_dev $::current(DEV)
        gotoFuncZero
        set port0 $::current(DEV)
        device $cu_dev -no_display
               
        set read_all 1
        set config_list [nvm_load 0 0 $read_all]
	set nvm_config [lindex $config_list 0]
	set default_config [lindex $config_list 1]

        # Setting portMax
        set portMax [GetMaxNumOfPorts]
       
    } else {
        set port0 1
        set portMax [GetDefaultMaxNumOfPorts $def_params(BOARD)] 
    }
   
    # print all 16 functions per-func configuration
    for {set func_num 0} {$func_num < 16} {incr func_num} {
    
    puts "Read configuration from function $func_num"
        
	# if port > portMax , no need to show port information any more
	# since all ports information was already shown.
	if {$func_num >= $portMax} {
		set do_not_show_per_port true
	}
	
	# Show shared information on device 0 only
	if {$func_num > 0} {
		set do_not_show_shared true
	}
	
        set currPort {}
	set currFunc {}
        if {!$def_dump} {
               set ::current(SPECIAL_NVM_MODE_FUNC_NUM) $func_num
               set ::current(SPECIAL_NVM_MODE_NUM_PORTS) $portMax
        } else {
            set curr_func $func_num
        }

        initGroupDisplay
        set menuD {}
	set any_id_shown false
        for {set i 0} {$i<[llength $IDs]} {incr i} {
		 
		if {![showField [lindex $IDs $i]]} {
			continue
		}
		if {[isFieldIgnored [lindex $IDs $i]]} {
			continue
		}
		set any_id_shown true
		set curr_id [lindex $IDs $i]
		
		puts "Read option $curr_id information"
		
		## Fill up group information display for each configuration ID
		if {!$def_dump} {
			addIdInfo $curr_id [get_feature_line $curr_id]
		} else {
			addIdInfo $curr_id [get_default_feature_line $curr_id $def_params(DID) $def_params(BOARD) $port_num $func_num]
		}
        }
	#if now id was shown, skip this device
	if {![set any_id_shown]} {                
		continue
	}
	
	# print each group information and current configuration
	set menuD [printGroupDisplay]
        set menuD #[string map {"\n" "\n#"} $menuD]
        lappend perDevMenu $menuD
	set any_id_shown false

        for {set i 0} {$i<[llength $IDs]} {incr i} {
        
		if {![showField [lindex $IDs $i]]} {
		continue
		}
		if {[isFieldIgnored [lindex $IDs $i]]} {
			continue
		}
		
		set id [lindex $IDs $i]
		puts "Read option $id data"
		if {!$def_dump} {
			set chip_num [getChipNum]
		} else {
			set chip_num $def_params(CHIP_NUM)
		}
		if {[isGlobal $id] && $func_num>0 || [lsearch -exact $noDefault $id]>-1 || \
			([isFieldExcluded $id $chip_num])} {
			continue
		}
		if {!$def_dump} {
			set formatted_val [get_formatted_val $id false]
			set any_id_shown true
		} else {
			set no_def_var false
			if {$no_def_var} {
				continue
			}
			set any_id_shown true
			set formatted_val [get_default_val $id $def_params(BOARD) $port_num $func_num no_def_var false]
		}
		if {[isGlobal $id]} {
			append shared "{$id=[set formatted_val]}\\\n"
		} else {
			if {[isPerFunc $id]} {
				append currFunc "{$id=[set formatted_val]}\\\n"
			} else {
				append currPort "{$id=[set formatted_val]}\\\n"
			}
		}
        }
	if {[set any_id_shown]} {
		lappend perPorts $currPort
		lappend perFunction $currFunc
	}
	puts "Completed reading configuration from function $func_num"
    }


    set do_not_show_shared $old_show_shared
    set do_not_show_per_port $old_show_per_port
    set do_not_show_per_function $old_show_per_function
}

# retruned an aligned line for the requested id
proc nvmcfgT::get_feature_line {id {show_not_applicable false} {force_not_applicable false}} {
        variable nvm_cfg
	
        set is_applicable true
        if {$force_not_applicable} {
		set is_applicable false
	}
        if {[isFieldExcluded $id]} {
                set is_applicable false 
        }
	if {!$is_applicable && !$show_not_applicable} {
		return {}
	}
        set line [format "%3d" $id]:

        if {[isGlobal $id]} {
                append line { [GLOB] }
        } elseif {[isPerPath $id] } {
                append line { [PATH] }
        } elseif {[isPerPort $id]} {
                append line { [PORT] }    
        } else {
                append line { [FUNC] }
        }
	
        append line "[align_feature_desc $id]: "
	if {$is_applicable} {
		append line [get_formatted_val $id]
	} else {
		append line "NOT APPLICABLE"
	}
        append line "\n"
        return $line
}

# retruned the default aligned line for the requested id
proc nvmcfgT::get_default_feature_line {id device_id board port_num func_num} {
    variable nvm_cfg
    if {![does_id_exist_for_chip $id $device_id]} {
        return {}
    }
    set line [format "%2d" $id]:

    if {[isGlobal $id]} {
        append line { [S] }
    } else {
        set appended 0
		if {[info exists nvm_cfg($id,per_func)]} {
			if {$nvm_cfg($id,per_func)} {
				append line { [F] }
				set appended 1
			}

		}
		if {$appended == 0} {
			append line { [P] }
		}
    }
    append line "[align_feature_desc $id]: "
    variable is_default_value
    append line [get_default_val $id $board $port_num $func_num is_no_default_value true]
    append line "\n"
    if {!$is_no_default_value} {
        return $line
    } else {
        return {}
    }
}

# Get value per chip type
# Input arguments:
# <chipType>: BB or AH or AHP
# <valStr>: <chipType>_<chipVal>
#
# e.g. BB_25G_AH_40G_AHP_40G
# Return:
# 25G for BB, 40G for AH, 40G for AHP
proc nvmcfgT::getValPerChipType {{chipType} {inStr}} {

	set retVal ""
	
	#Parse input string into value and units
	scan $inStr "%s %s" valStr unitsStr
	
	#Find the location index of chipType in valStr	
	set chipLen [string length $chipType]
	
	#add suffix _ to distinguish between AH and AHP
	set chipType_ $chipType	
	append chipType_ "_"
	set startIndex [string first $chipType_ $valStr]
	
	if {$startIndex != -1} {
		#Find the delimiter "_" after chip_type and chip_value
		set endIndex [string first "_" $valStr [expr {$startIndex+$chipLen+1}]]
		
		#if last field, set endIndex to string length
		if {$endIndex == -1} {
			set endIndex [string length $valStr]
		}
		
		#Get the <chipType>_<Val> string e.g. BB_25G
		set chipValStr [string range $valStr $startIndex $endIndex]

		#Remove chipType_ prefix
		set retVal [string trim [string replace $chipValStr 0 $chipLen ""] "_"]

		if {[info exists unitsStr]} {
			append retVal " $unitsStr"
		}
	}
	
	return $retVal
}


# transfer an allowed list to a string with all values and their enums
proc nvmcfgT::allowedListToString {id} {
    variable nvm_cfg
    variable ChipIdToChipNum
    upvar #0 nvmcfgT::$nvm_cfg($id,allowedList) aList
    set ans ""
    
    #Get chip number (57980/57940)
    set chip_num [getChipNum]
    
    set keys [lsort -integer [array names aList]]
    
    set formats [stringFormatToList $id]
    if {[regexp {%\d*x} [lindex $formats 0]]} {
        set base %x
    } else {
        set base %d
    }      
    
    for {set i 0} {$i<[llength $keys]} {incr i} {
	set is_bb_val 0
	set is_ah_val 0
	set is_ahp_val 0	
	
	#Check if item is for specific chip
	set bb_val [getValPerChipType "BB" $aList([lindex $keys $i])]
	set ah_val [getValPerChipType "AH" $aList([lindex $keys $i])]
	set ahp_val [getValPerChipType "AHP" $aList([lindex $keys $i])]
	
	if {[string length $bb_val]} {
		set is_bb_val 1
	}
	if {[string length $ah_val]} {
		set is_ah_val 1
	}
	if {[string length $ahp_val]} {
		set is_ahp_val 1
	}
	
	#if value is not per chip, then it's for all chip types
	#show it as is
	if {!$is_bb_val && !$is_ah_val && !$is_ahp_val} {
		set item $aList([lindex $keys $i])
	} else {
		if {$chip_num == "57980"} {
			if {$is_bb_val} {
				#If this is BB value, show it only on BB.
				set item $bb_val
			} else {
				#If this is the last option value, chop the last comma
				if {[expr {$i+1}]==[llength $keys]} {
					set ans [string trimright $ans ", "]
				}
				continue
			}
		} elseif {$chip_num == "57940"} {
			if {$is_ah_val} {
				#If this is AH value, show it only on AH.
				set item $ah_val
			} else {
				#If this is the last option value, chop the last comma
				if {[expr {$i+1}]==[llength $keys]} {
					set ans [string trimright $ans ", "]
				}
				continue
			}
		} elseif {$chip_num == "55000"} {
			if {$is_ahp_val} {
				#If this is AHP value, show it only on AHP.
				set item $ahp_val
			} else {
				#If this is the last option value, chop the last comma
				if {[expr {$i+1}]==[llength $keys]} {
					set ans [string trimright $ans ", "]
				}
				continue
			}
		} else {
			puts "Unknown chip num $chip_num"
		}
	}
			
        if {[string equal $nvm_cfg($id,listType) "bitfield"]} {
            append ans [format "%s(0x%x)" $item [lindex $keys $i]]
        } else {
            append ans [format "%s($base)" $item [lindex $keys $i]]
        }
	
        if {[expr {$i+1}]!=[llength $keys]} {
            append ans ", "
        }
    }
    append ans "\}"
    return $ans
}

# align the feature description to fit the screen
proc nvmcfgT::align_feature_desc {id} {
    variable nvm_cfg
    set ans [align_text $nvm_cfg($id,name) "        " 47]
    if {[info exists nvm_cfg($id,allowedList)]} {

        set aList [allowedListToString $id]

        set lineSize [string length [string trimright $ans " "]]
        if {[expr [string length $aList]+$lineSize+2<47]} {
             set ans [format "%s \{%s" [string trimright $ans " "] [align_text $aList "        " [expr 45-$lineSize]]]
        } else {
            set ans [format "%s\n           \{%s" [string trimright $ans] [align_text $aList "            " 47]]
        }

    }
    return $ans
}
# returned the id's value, if verbose is true return the string from the allowed list (if exists)
# otherwise, return the numeric value
# (if the value is a string, it will be return as is.)
proc nvmcfgT::get_formatted_val {id {verbose true} {dev_idx -1} {port_max 4}} {
    variable IDs
    variable nvm_cfg
    variable get_def_cfg

    if {![does_id_exist_for_chip $id [getChipNum]]} {
        puts "Unknown ID - $id, ignoring $id=."
        return false
    }
    
    set values [lreverse [get_elements $id $dev_idx $port_max]]
    
    return [format_values $id $values $verbose]
}

# Making sure that the device ID is not in the excluded list for the specified ID.
proc nvmcfgT::does_id_exist_for_chip {id chip} {
    variable IDs
    variable nvm_cfg
        
    if {[lsearch -exact $IDs $id]==-1 || ([isFieldExcluded $id $chip])} {
        return false
    }

    return true

}

# Return the default value for Number of ports
proc nvmcfgT::get_default_NumPorts {board} {
    variable IDs
    variable nvm_cfg
    variable no_def_var
    
    # Miri TBD: Check if Hide port option enabled
    return 4;
}


# Verifies that a specific board and port have a default val for the chosen ID. (no_def_var is appropriately
# updated.)
# The returned value is the default val
proc nvmcfgT::verify_default_value {id board port_num func_num no_def_var} {
        variable IDs
        variable nvm_cfg
        variable boards

        upvar #0 nvmcfgT::$board boardD
        upvar #0 nvmcfgT::default defaultD
        upvar 1 $no_def_var no_default

	set no_default false

        if {[lsearch -exact $boards $board]==-1} {
                set no_default true
                error "Unknown board to check."
	}
	
	if {[isGlobal $id] } {
		# Get default value of global id
		if {[info exist boardD($id)]} {
			return $boardD($id)
		} elseif {[info exist defaultD($id)]} {
			return $defaultD($id)
		} else {
			set no_default true
			return false
		}
	} elseif {[isPerPort $id] } {
		# Get default value of per-port id
		if {[info exist boardD($id)]} {
			return $boardD($id)
		} elseif {[info exists boardD($id,$port_num)]} {
			return $boardD($id,$port_num)
		} elseif {[info exist defaultD($id)]} {
			return $defaultD($id)
		} elseif {[info exists defaultD($id,$port_num)]} {
			return $defaultD($id,$port_num)
		} else {
			set no_default true
			return false
		}
	} else {
		# Get default value of per-func id
		if {[info exist boardD($id)]} {
			return $boardD($id)
		} elseif {[info exists boardD($id,$func_num)]} {
			return $boardD($id,$func_num)
		} elseif {[info exist defaultD($id)]} {
			return $defaultD($id)
		} elseif {[info exists defaultD($id,$func_num)]} {
			return $defaultD($id,$func_num)
		} else {
			set no_default true
			return false
		}
		
	}
}

# returned the id's default value, if verbose is true return the string from the allowed list (if exists)
# otherwise, return the numeric value
# (if the value is a string, it will be return as is.)
proc nvmcfgT::get_default_val {id board port_num func_num no_def_var {verbose true}} {
    variable IDs
    variable nvm_cfg
    variable boards
    variable default_dump_params
    set did false
    if {[info exists default_dump_params(DID)]} {
	set did $default_dump_params(DID)
    }

    upvar #0 nvmcfgT::$board boardD
    upvar #0 nvmcfgT::default defaultD
    upvar 1 $no_def_var no_default

    set no_default false

    set defVal [verify_default_value $id $board $port_num $func_num no_default]

    if {[lsearch -exact $boards $board]==-1} {
        set no_default true
        error "Unknown board to check."
	}

    if {$no_default} {
        return false
    }

    set values [checkUserInput $id $defVal $did]
    if {[string equal $values false]} {
	set no_default true
        return false
    }
    return [format_values $id [lreverse $values] $verbose]
}

proc nvmcfgT::format_values {id values {verbose true}} {
    variable nvm_cfg
    variable IDs
   
    set formatVal [string trim $nvm_cfg($id,stringFormat) "^$"]
    regsub -all {([(]\?:.+?[)]\?)} $formatVal {} formatVal
    
    set formats [stringFormatToList $id]
   
    if {[string equal [lindex $formats 0] "string"]} {        
        set val ""
        set values [lreverse $values]
	if {![is_string_array $id]} {                
                if {[regexp {[(]\.} $nvm_cfg($id,stringFormat)]} {
                        #string = array of chars                        
                        set element_size 8
                        set no_elements [expr {$nvm_cfg($id,size)/$element_size}]
                } elseif {$nvm_cfg($id, elementSize) > 0} {
                        # Calculate number of elements by dividing size/elementSize
                        set no_elements [expr {$nvm_cfg($id,size)/$nvm_cfg($id, elementSize)}]
                        set element_size $nvm_cfg($id, elementSize)                                        
                } else {
                        set no_elements 1
                        set element_size $nvm_cfg($id, elementSize)
                }
                for {set i 0} {$i<$no_elements && [lindex $values $i]!=0} {incr i} {
			append val [format %c [lindex $values $i]]
		}
		regsub {[(][^)]+[)]} $formatVal $val formatVal
	} else {                
                # array of strings
                set val ""
                for {set str_num 0} {$str_num < $no_elements} {incr str_num} {

                        	if {$str_num > 0} {
                        		append val ":"
                        	}
                        	set curr_values [lindex $values $str_num]
                        	set i 0
                        	for {} {$i< [llength $curr_values] && [lindex $curr_values $i]!=0} {incr i} {
                        		append val [format %c [lindex $curr_values $i]]
                        	}
                        	if {$i == 0} {
                        		append val ""
                        	}

                        	set formatVal $val
                        }
	}
    } elseif {[llength $formats]==[llength $values]} {
        if {[info exist nvm_cfg($id,allowedList)] && [string equal $nvm_cfg($id,listType) "enum"] && $verbose} {
            variable $nvm_cfg($id,allowedList)
            set formatVal ""
	    
            for {set i 0} {$i<[llength $formats]} {incr i} {
		
                if {[info exist $nvm_cfg($id,allowedList)([lindex $values $i])]} {
			set strVal [subst $$nvm_cfg($id,allowedList)([lindex $values $i])]
			set chip_num [getChipNum]			
			set is_bb_val 0
			set is_ah_val 0
			set is_ahp_val 0	
			
			#Check if item is for specific chip
			set bb_val [getValPerChipType "BB" $strVal]
			set ah_val [getValPerChipType "AH" $strVal]
			set ahp_val [getValPerChipType "AHP" $strVal]
			
			if {[string length $bb_val]} {
				set is_bb_val 1
			}
			if {[string length $ah_val]} {
				set is_ah_val 1
			}
			if {[string length $ahp_val]} {
				set is_ahp_val 1
			}
			#if value is not per chip, then it's for all chip types
			#show it as is
			if {!$is_bb_val && !$is_ah_val && !$is_ahp_val} {
				append formatVal $strVal
			} else {
				if {$chip_num == "57980"} {
					if {$is_bb_val} {
						#If this is BB value, show it only on BB.
						append formatVal $bb_val
					}
			
				} elseif {$chip_num == "57940"} {
					if {$is_ah_val} {
						#If this is AH value, show it only on AH.
						append formatVal $ah_val
					}
				} elseif {$chip_num == "55000"} {
					if {$is_ahp_val} {
						#If this is AHP value, show it only on AHP.
						append formatVal $ahp_val
					}
				} else {
					puts "Unknown chip num $chip_num"
				}
			}
                } else {
			append formatVal [format "Unknown (0x%X)" [lindex $values $i]]
                }
                if {[expr $i+1<[llength $formats]]} {
			append formatVal :
                }
            }
        } else {
            if {([llength $formats] == 1) && [string match "*x" $formats]} {
                set formatVal "0x[format [string tolower $formats] $values]"
            } else {
                for {set i 0} {$i<[llength $formats]} {incr i} {
                    regsub {[(][^)]+[)]} $formatVal [format [string tolower [lindex $formats $i]] [lindex $values $i]] formatVal
                }
            }
        }
    } else {
        error "The string format define different amount of elements ([llength $formats]) than the feature holds [llength $values].[lindex $formats 0]"
    }
    
    return $formatVal
}

# recieve the id and data enter by the user
# verify the validity of the data and if so set the data
proc nvmcfgT::set_feature_val {id data {dev_idx -1} {port_max 4}} {
	variable nvm_config
        variable default_config
        variable verify_cfg
        variable MNM_capability
        variable svid_val
        
	if {[isFieldIgnored $id]} {
		return false
	}
	set values [checkUserInput $id $data]
        
	if {[string equal $values "false"]} {
	    return false
	} else {
	    set res [set_elements $id $values $dev_idx $port_max]
            if {($res == false) && $verify_cfg} {
                puts "   Expected cfg [format_values $id [lreverse $values]]\n"
                return $res
            }
	}
        return true
}

# verify the user input by checking the following:
# The id is known.
# The data fits the regualr expression (string format)
# if an allowed list exists ensure data is a member of it.
# ensure data doesn't exceed the field
# ensure data is a multiple of <multiple of> if exists
proc nvmcfgT::checkUserInput {id data {given_chip_num false}} {
    variable nvm_cfg
    variable IDs
    variable noDefault   
    variable ChipIdToChipNum
    variable MNM_capability
    variable svid_val
        
    # if ID is not in the IDs list return false.
    # This call must be early in the function because otherwise function will
    # exit with error other than return false, and then if a nvm_cfg script
    # will include one ID not in the list other assignments will not take place.
    # It is important for us that other assignments will take place because if
    # script was generated before we removed options from nvm cfg it wouldn't
    # pass
    if {[lsearch -exact $IDs $id]==-1 || ([isFieldExcluded $id $given_chip_num])} {
        puts "Unknown ID - $id, ignoring $id=$data."
        return true
    }

    if {[isFieldIgnored $id]} {
	puts "Field $id is currently ignored."
	return true
    }

    if {$id == 118} {
        # read device id from MISCS_REG_CHIP_NUM      
        set did [reg read 0x976c]
        # find current chip num 57940/57980
        set chip_num $ChipIdToChipNum($did)

        # check requested new device id
        if {![string match "*x*" $data]} {
                set data "0x[format "%d" $data]"
        }

        set new_did [expr {$did & 0xff00} | $data]
        if {[info exist ChipIdToChipNum([format "0x%x" $new_did])]} {
            set new_chip_num $ChipIdToChipNum([format "0x%x" $new_did])
        } else {
            puts "ERROR: Missing $new_did in list ChipIdToChipNum !!"
            return false
        }
        if {$new_chip_num != $chip_num} {
            puts "ERROR: Option 118 requested DID suffix is not for this chip ($chip_num) !!"
            return false
        }
    }

    upvar #0 nvmcfgT::default defaults
    set regEx $nvm_cfg($id,stringFormat)
    set formats [stringFormatToList $id]
    set element_size [expr {abs($nvm_cfg($id,elementSize))}]
    
    # Calculate number of elements by dividing size/elementSize
    if {$element_size > 0} {
        set no_elements [expr {$nvm_cfg($id,size)/$element_size}]
    } elseif {[getMacSize $id] > 0} {
        #MAC address
        set no_elements 6
        set element_size 8
    } else {
        set no_elements 1
        set element_size $nvm_cfg($id,size)
    }    

    if {[string equal [lindex $formats 0] "string"]} {
        set  no_elements 1
    }
    
    if {[regexp $regEx $data]} {
        for {set i 0} {$i<$no_elements} {incr i} {
            lappend vars val($i)
        }

        eval [subst {regexp \{$regEx\} \{$data\} dc $vars}]

        if {[string equal [lindex $formats 0] "string"]} {
            set i 0
            for {} {$i<[string length $val(0)]} {incr i} {
               lappend ans [scan [string index $val(0) $i] %c]
            }
            if {$i!=$no_elements} {
                lappend ans 0
            }
            return $ans
        } else {
            for {set i 0} {$i<$no_elements} {incr i} {

                set value [scan $val($i) [string tolower [lindex $formats $i]]]
                lappend ans $value
                if {[info exist nvm_cfg($id,allowedList)] && [string equal $nvm_cfg($id,listType) "enum"]} {
                    variable $nvm_cfg($id,allowedList)
                    upvar #0 nvmcfgT::$nvm_cfg($id,allowedList) aList
                    if {![info exist $nvm_cfg($id,allowedList)($value)]} {
                        puts "The value for id $id, is illegal, the value must be member of the allowed list."
                        puts "The list: \{[align_text $aList "        " 69]"
                        puts "Your value was: $val($i)"
                        return false
                    } else {
                        continue
                    }
                }
                if {$value >= [expr pow(2,$element_size)]} {                    
                    puts "The value of element $i for id $id, is illegal, the value must be less than [expr int(pow(2,$element_size))]."
                    return false
                }                
                if {[info exists nvm_cfg($id,multipleOf)] && [expr {$value % $nvm_cfg($id,multipleOf)}]} {
                    puts "All elements given in the value of id $id, must be multiple of $nvm_cfg($id,multipleOf)."
                    puts "Recheck element $i."
                    return false
                }

            }
            return [lreverse $ans]
        }
    } else {
        puts "The data you enter for id $id, is not in the correct string format."
        if {[lsearch -exact $noDefault $id]!=-1} {
            if {[string equal $nvm_cfg($id,size) mac]} {
                puts "Example: 00:10:18:AB:CD:EF."
            } else {
                puts "Expected string in this format: $regEx."
            }
        } else {
            puts "Example: $defaults($id)."
        }
        puts "The data was: $data."
        return false
    }
}
# split the string format to a list
# used by scan for each data element
proc nvmcfgT::stringFormatToList {id} {
    variable nvm_cfg
    set withStr false
    regsub -all {\?:} $nvm_cfg($id,stringFormat) {~@~} stringFormat
    set formats [split  $stringFormat ":"]
    
    for {set i 0} {$i<[llength $formats]} {incr i} {
        if {$withStr} {
            # String array
	    set nvm_cfg([set id],is_string_array) true
        }
        if {[regexp {[(]\[0-9a-fA-F\]} [lindex $formats $i]]} {
            if {[regexp {[(]\[0-9a-fA-F\][\{](?:\d+,)?(\d+)[\}]} [lindex $formats $i] align align]} {
                lappend ans "%0[set align]x"
            } else {
                lappend ans "%x"
            }
        } elseif {[regexp {[(]\\d} [lindex $formats $i]]} {
            if {[regexp {[(]\\d[\{](?:\d+,)?(\d+)[\}][)]} [lindex $formats $i] align align]} {
                lappend ans "%0[set align]d"
            } else {
                lappend ans "%d"
            }        
        } elseif {[regexp {[(]\.} [lindex $formats $i]]} {
            if {$i} {
                # String array
		set nvm_cfg([set id],is_string_array) true
            }
            set withStr true
            lappend ans "string"
        } else {
            error "Unknown stringFormat $nvm_cfg($id,stringFormat) element $i $formats"
        }
    }
    return $ans
}

#returns 0 is non-MAC
proc nvmcfgT::getMacSize {id} {
    variable nvm_cfg
    if {[string equal $nvm_cfg([set id],size) mac] == 0} {
        return 0
    }
    set sform $nvm_cfg($id,stringFormat)
    set sform [split $sform :]
    return [expr {[llength $sform] / 2}]
}

# Recieve a list with the data of the elements  and id
# set the elements for that id
proc nvmcfgT::set_elements {id elems {dev_idx -1} {port_max 4}} {
    variable nvm_cfg
    variable nvm_config
    variable default_config
    variable announcements
    variable verify_cfg
    variable get_def_cfg

    set setElems 0
    set offset $nvm_cfg($id,offset)                    
                              
    if {[regexp {[(]\.} $nvm_cfg($id,stringFormat)]} {
        #Handle string as array of char elements
        set element_size 8
    } else {
        set element_size [expr {abs($nvm_cfg($id,elementSize))}]
    }
        
    # Calculate number of elements by dividing size/elementSize
    if {$element_size > 0} {
        set no_elements [expr {$nvm_cfg($id,size)/$element_size}]
    } elseif {[getMacSize $id] > 0} {
        #MAC address
        set no_elements 6
        set element_size 8
    } else {
        set no_elements 1
        set element_size $nvm_cfg($id,size)
    }    
   
    set isMac [string equal $nvm_cfg($id,size) mac]
   
    if {$isMac} {
        if {[getMacSize $id] != 8} {
      
            if {[getMacSize $id] != 6} {
                puts "ERROR: field $id contains [getMacSize $id] elements. Defaulting to 6."
            }
            set temp [lrange $elems 0 3]
            set elems [linsert $temp 0 [lindex $elems 4] [lindex $elems 5] 0 0]
            set no_elements 8
        } else {            
            set temp [lrange $elems 0 3]
            set elems [linsert $temp 0 [lindex $elems 4] [lindex $elems 5] [lindex $elems 6] [lindex $elems 7]]
            set no_elements 8
        }
        
    } elseif {![string equal [stringFormatToList $id] {string}] &&
              ($element_size<32) && ([expr {$element_size*$no_elements}]>32) } {
        # field which are larger then 32 bits and are build from elements which are smaller then 32 bits
        # are array which must be written in Big endianity
        for {set i 0} {$i<[llength $elems]-1} {incr i [expr {32/$element_size}]} {
            eval [subst {lappend temp [lreverse [lrange $elems $i [expr $i+32/$size-1]]]}]
        }
        set elems $temp
    }
    
    #Forward the type glob/path/port/func to nvm_set_bits inorder to add the
    #offset of the relevant block.
    set type $nvm_cfg($id,entity_name)
   
    while {$no_elements!=$setElems && $setElems !=[llength $elems]} {  

        if {$verify_cfg} {
            # Verify option value in NVM_CFG1 and DEF_CFG w/o setting
            set val [nvm_get_bits $nvm_config $type $offset $element_size $dev_idx $port_max]
            set def_val [nvm_get_bits $default_config $type $offset $element_size $dev_idx $port_max]
           
            if {($val != [lindex $elems $setElems]) ||
                ($def_val != [lindex $elems $setElems])} {

                set ::current(VERIFY_CFG_FAILED) 1
                if {[isGlobal $id]} {
                    puts "ID $id \[GLOB\]: \"$nvm_cfg($id,name)\" mismatch:"
		} elseif {[isPerPort $id] } {
                    set func_num $::current(SPECIAL_NVM_MODE_FUNC_NUM)
                    set num_ports $::current(SPECIAL_NVM_MODE_NUM_PORTS)
                    set port_num [expr {$func_num % $num_ports}]
		    puts "ID $id \[PORT\] for port $port_num: \"$nvm_cfg($id,name)\" mismatch:"
		} else {
                    set func_num $::current(SPECIAL_NVM_MODE_FUNC_NUM)
		    puts "ID $id \[FUNC\] for func $func_num: \"$nvm_cfg($id,name)\" mismatch:"
		}
		
		puts "   Existing cfg [get_formatted_val $id true $dev_idx $port_max]"
                set get_def_cfg 1
                puts "   Default cfg  [get_formatted_val $id true $dev_idx $port_max]"
                set get_def_cfg 0
                return false
            }
        } else {
            # If device was selected, change NVM bits at the location of the selected dev_idx
            if {$dev_idx != -1} {
                nvm_set_bits $nvm_config $type $offset $element_size [lindex $elems $setElems] $dev_idx $port_max
            } else {
                #otherwise change NVM bits of the current device
                nvm_set_bits $nvm_config $type $offset $element_size [lindex $elems $setElems]
            }
        }
        
        incr offset $element_size
        incr setElems
    }
    if {[info exist nvm_cfg($id,announce)]} {
        append announcements $nvm_cfg($id,announce)
        append announcements "\n"
    }
    return true
}
proc nvmcfgT::is_string_array {id} {

	variable nvm_cfg
	if {[catch {set l $nvm_cfg([set id],stringFormat)}]} {

		return false
	}
	set l [split $l :]
	if {[llength $l] < 2} {
		return false
	}
	set first_item [lindex $l 0]
	if {[regexp {\.\{} $first_item]} {
		return true
	} else {
		return false
	}
}

proc nvmcfgT::read_byte_list {type offset size} {
	variable nvm_config

	set result {}
	for {set read_size 0} {$read_size < $size } { incr read_size 8} {
		set current_offset [expr {$offset + $read_size}]
		lappend result [nvm_get_bits $nvm_config $type $current_offset 8]
	}

	return $result
}

# Return a list with the data of the elements
proc nvmcfgT::get_elements {id {dev_idx -1} {port_max 4}} {
    variable nvm_cfg
    variable nvm_config
    variable default_config
    variable get_def_cfg
    
    set readElems 0
    set offset $nvm_cfg($id,offset)
    
    if {[regexp {[(]\.} $nvm_cfg($id,stringFormat)]} {
        #Handle string as array of char elements
        set string_array true
        set element_size 8
    } else {
        set element_size [expr {abs($nvm_cfg($id,elementSize))}]
    }
        
    # Calculate number of elements by dividing size/elementSize
    if {$element_size > 0} {
        set no_elements [expr {$nvm_cfg($id,size)/$element_size}]
    } elseif {[getMacSize $id] > 0} {
        #MAC address        
        set no_elements 6
        set element_size 8
    } else {
        set no_elements 1
        set element_size $nvm_cfg($id,size)
    }    
        
    set isMac [string equal $nvm_cfg($id,size) mac]
    if {$isMac} {
        set no_elements 8
    }
    
    set string_array false
    if {[is_string_array $id]} {
	set string_array true
    }
    
    #Forward the type glob/path/port/func to nvm_set_bits inorder to add the
    #offset of the relevant block.
    set type $nvm_cfg($id,entity_name)
    
    while {$no_elements!=$readElems} {
	if {$string_array} {                
		set val [read_byte_list $type $offset $element_size]                
	} else {    
               # If device was selected, read NVM bits at the location of the selected dev_idx
                if {$dev_idx != -1} {
                    if {$get_def_cfg == 0} {
                        set val [nvm_get_bits $nvm_config $type $offset $element_size $dev_idx $port_max]
                    } else {
                        set val [nvm_get_bits $default_config $type $offset $element_size $dev_idx $port_max]
                    }
       	     
                } else {
                    if {$get_def_cfg == 0} {
                        #otherwise read NVM bits of the current device
                        set val [nvm_get_bits $nvm_config $type $offset $element_size]
                    } else {
                        set val [nvm_get_bits $default_config $type $offset $element_size]
                    }
                }         
        }
                
        lappend ans $val        
        incr offset $element_size
        incr readElems
    }
    
    if {$isMac} {        
        set temp $ans
        set ans [lrange $ans 4 end]
        set ans [linsert $ans end [lindex $temp 0] [lindex $temp 1]]
        if {[getMacSize $id] == 8} {
            set ans [linsert $ans end [lindex $temp 2] [lindex $temp 3]]
        }
    } elseif {![string equal [stringFormatToList $id] {string}] &&
                ($element_size < 32) && ([expr {$element_size *$no_elements}] >32)} {
        # field which are larger then 32 bits and are build from elements which are smaller then 32 bits
        # are array which must be read in Big endianity
        for {set i 0} {$i<[llength $ans]-1} {incr i [expr {32/$element_size}]} {
            eval [subst {lappend temp [lreverse [lrange $ans $i [expr $i+32/$element_size-1]]]}]
        }
        set ans $temp
    }
    
    return $ans
}

# align the text to lines of size
# the firstLine can have different length
# the prefix is added to all lines below the first
proc nvmcfgT::align_text {text prefix size {firstLine -1}} {
    set col 0
    set aligned ""
    if {$firstLine!=-1} {
        set lineSize $firstLine
    } else {
        set lineSize $size
    }

    for {set i 0} {$i<[llength $text]} {incr i} {
        if {$col+[string length [lindex $text $i]]<$lineSize} {
            append aligned [lindex $text $i]
            incr col [string length [lindex $text $i]]
        } else {
            if {[string length [lindex $text $i]]>$size} {
                error "You enter strings which are larger than the line size: [lindex $text $i]."
            }
            set lineSize $size
            append aligned [format "\n%s%s" $prefix [lindex $text $i]]
            set col [string length [lindex $text $i]]
        }
        if {$col<$size} {
            append aligned " "
            incr col 1
        }
    }
    for {set i 0} {$i<[expr $size-$col]} {incr i} {
        append aligned " "
    }
    return $aligned
}

# reverse a list
proc nvmcfgT::lreverse {originList} {
    for {set i [expr [llength $originList]-1]} {$i>=0} {incr i -1} {
        lappend ans [lindex $originList $i]
    }
    return $ans
}

# set the defaults for a specic board
# first look for the default in that board's array (id as key)
# otherwise checks if the value exists per port (id,port as key)
# otherwise checks if the value exists in the default's array
# otherwise checks if the value exists per port in the default's array (id,port as key)
proc nvmcfgT::set_board_default_vals {board} {
    variable IDs
    variable nvm_cfg
    variable portMax
    variable funcs
    variable noDefault
    variable nvm_config
    variable chipnumPerBoard
        
    upvar #0 nvmcfgT::$board boardD
    upvar #0 nvmcfgT::default defaultD

    set curr_device $::current(DEV)
    set read_all 1
    set write_all 1

    # setting the port to be port0
    gotoFuncZero
    set func0 $::current(DEV)

    set chipNum [getChipNum]
    if {$chipNum != $chipnumPerBoard($board)} {
        puts "ERROR: Request configuration is for $chipnumPerBoard($board), not for this chip ($chipNum) !"
        return
    }

    set no_ports [GetDefaultMaxNumOfPorts $board]

    set num_vfs_idx [find_id_for_field {Number of VFs per PF}]
    set mbi_ver_idx [find_id_for_field {MBI version}]
    set mbi_date_idx [find_id_for_field {MBI date}]
   
    # read the whole NVM configuration
    set config_list [nvm_load 0 0 $read_all]
    set nvm_config [lindex $config_list 0]
   
    for {set func_num 0} {$func_num < 16} {incr func_num} {
	
        set dev_idx [expr {$func_num +1}]
	set port_num [expr {$func_num % $no_ports}]

	# Go through nvm cfg option IDs
        for {set i 0} {$i<[llength $IDs]} {incr i} {
            set id [lindex $IDs $i]
                
	    # Glob parameters can be configured on func 0 only
            if {([isGlobal $id] && ($func_num > 0)) || \
                ([lsearch -exact $noDefault $id] > -1) || \
                ([isFieldExcluded $id])} {
                continue
            }
	    
	    # CQ82444 - Don't over-write MBI version & date
	    if {($id == $mbi_ver_idx) || ($id == $mbi_date_idx)} {
		continue
	    }
		
	    
	    # Per-port parameters should be configured once per port
	    if {([isPerPort $id] && ($func_num >= $no_ports)) } {
		continue
	    }
	    
            set no_default false
            set defVal [verify_default_value $id $board $port_num $func_num no_default]
            
            if {$no_default} {
                error "couldn't find a value to verify for id $id , func $func_num"
            }   
            
            # CQ79152 Avoid total num of VFs > 240 by setting 
            # num Vfs 8 instead of 16 on two last functions.
            if {($id == $num_vfs_idx) && ($defVal == 16)} {
                if {($func_num == 14) || ($func_num == 15)} {
                    set defVal 8
                }
            }        
                        
            set_feature_val $id $defVal $dev_idx $no_ports
        }   
    }

    nvm_store $nvm_config 0 0 $write_all
    device $curr_device -no_display
}

# return 0 if all devices for that card are not loaded
# else return first device with driver loaded in that card
proc nvmcfgT::is_driver_loaded {} {
    if {$::current(VIRTUAL_NVM_MODE) == "1"} {
	variable funcs
	set funcs $::current(TOTAL_DEV)
	return 0
    }

    set curr_device $::current(DEV)
    gotoFuncZero
    set dev $::current(DEV)
    set ans none
    variable funcs
    set funcs 0
    while {1} {
        incr funcs
        if {[string equal $::current(DRV_STATE) "RUNNING"]} {
            device $curr_device -no_display
            return $dev
        }
        if {$dev==$::current(TOTAL_DEV)} {
            device $curr_device -no_display
            return 0
        }
        incr dev
        if {[catch {device $dev -no_display}]} {
            continue
        }
        if {[isFuncZero]} {
            device $curr_device -no_display
            return 0
        }
    }

}

proc nvmcfgT::show_boards {} {
        variable boards
        
        puts "Supported boards are:\n $boards"
}



# ---------------------------------------------------
# This proc program MAC addresses to requested dev
# ---------------------------------------------------
proc nvmcfgT::set_mac_address {dev_idx mac_addr do_load_cfg do_store_cfg} {
	
	variable nvm_cfg		
	variable nvm_config
        variable verify_cfg
	
        set read_all 1
        set write_all 1
        set verify_cfg 0
	
        if {$do_load_cfg} {
            set config_list [nvm_load 0 0 $read_all]
            set nvm_config [lindex $config_list 0]
        }
				
	#MAC address id
	set id 1
        set no_ports [GetMaxNumOfPorts]
	set_feature_val $id $mac_addr $dev_idx $no_ports		     		
		
        if {$do_store_cfg} {
            #Store to NVM after changing MAC addresses for all functions
            nvm_store $nvm_config $dev_idx $no_ports $write_all 
        }
}

# --------------------------------------------------------------
# This proc program WWN port/node MAC addresses to requested dev
# --------------------------------------------------------------
proc nvmcfgT::set_wwn_mac_address {dev_idx mac_addr} {
	
	variable nvm_cfg		
	variable nvm_config
        variable verify_cfg
       
        set verify_cfg 0
	                
	set no_ports [GetMaxNumOfPorts]
        
	#WWN MAC address option ids
	set node_wwn_mac_addr_opt 93
	set port_wwn_mac_addr_opt 94
	
	set_feature_val $node_wwn_mac_addr_opt $mac_addr $dev_idx $no_ports
	set_feature_val $port_wwn_mac_addr_opt $mac_addr $dev_idx $no_ports
}

# --------------------------------------------------------------
# This proc program LLDP MAC addresses to requested port
# --------------------------------------------------------------
proc nvmcfgT::set_lldp_mac_address {dev_idx mac_addr do_load_cfg do_store_cfg} {

        variable nvm_cfg		
        variable nvm_config
        variable verify_cfg
        
        set read_all 1
        set write_all 1
        set verify_cfg 0

        if {$do_load_cfg} {
            set config_list [nvm_load 0 0 $read_all]
            set nvm_config [lindex $config_list 0]
        }

        #WWN MAC address option ids
        set lldp_mac_addr_opt 99
        set no_ports [GetMaxNumOfPorts]
        set_feature_val $lldp_mac_addr_opt $mac_addr $dev_idx $no_ports
       
        if {$do_store_cfg} {
            #Store to NVM after changing MAC addresses for all functions
            nvm_store $nvm_config $dev_idx $no_ports $write_all
        }
}

proc nvmcfgT::set_driversim_mode {} {
	set chip_num [getChipNum]
       if {$chip_num == 57980} {
       	puts "Applying Driversim mode for BB"
       	nvm cfg -all 9=0 15=0 23=1 59=0 105=1 128=3 92=0 83=5 118=0x29 273=1 26=0
	} elseif {$chip_num == 57940} {
		puts "Applying Driversim mode for AH"
		nvm cfg -all 9=0 15=0 23=1 59=0 105=1 128=3 92=0 191=5 118=0x70 273=1 26=0
	} elseif {$chip_num == 55000} {
		puts "Applying Driversim mode for E5"
		nvm cfg -all 9=0 15=0 23=1 59=0 105=1 128=3 92=0 191=5 118=0x70 273=1 26=0
	}
}
