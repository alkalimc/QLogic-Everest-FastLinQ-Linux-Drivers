## This script is used to convert binary NVM_CFG file into cfg tcl file
## with similar format as nvm_cfg -dump <outfile>
##
## Inputs: <chip number (57980/57940)> <nvm cfg bin file> <nvm meta bin file> <outfile>

proc stdout { switch { file "" } } {
 # do nothing
}

proc version {} {
    return 0.0.0.0
}

proc device {{dev_idx 1} {opt 0}} {   
    set ::current(DEV) $dev_idx
    set ::current(SPECIAL_NVM_MODE_FUNC_NUM) [expr {$dev_idx -1}]
}

proc nvm_load {{dev_idx} {num_ports} {read_all}} {    
    variable nvm_cfg_file
    
    set res {}
    set fp [open $nvm_cfg_file r]
    fconfigure $fp -translation binary
    set nvm_data [read $fp]   
    close $fp
   
    lappend res $nvm_data   
    return $res
}


## Read value bits of requested option
## Inputs:
## nvm_config: NVM binary data (output of nvm_load)
## type: option type (glob, path, port or func)
## offset: offset from begining of NVM data in bits
## size: number of bits
proc nvm_get_bits { {nvm_config} {type} {offset} {size}} {
    
    set NVM_CFG_PATH_OFFSET 0x140
    set NVM_CFG_PORT_OFFSET 0x230
    set NVM_CFG_FUNC_OFFSET 0xb90
    set NVM_CFG_PATH_SIZE 120
    set NVM_CFG_PORT_SIZE 600
    set NVM_CFG_FUNC_SIZE 80
    
    set pci_func $::current(SPECIAL_NVM_MODE_FUNC_NUM)    
    set num_ports $::current(SPECIAL_NVM_MODE_NUM_PORTS)
    
    # Calculate the current path/port/func
    if {$::current(CHIP_NUM) == 57980} {	
	set path [expr {$pci_func % 2}]
	set port [expr {$pci_func % $num_ports}]
	set func [expr {(($pci_func >> 1) | (($pci_func & 0x1) << 3))}]
    } else {	
	set path 0
	set port [expr {$pci_func % $num_ports}]
	set func $pci_func
    }
    
        
    # Add offset of the entity (glob/path/port/func)
    if {$type == "glob"} {        
        set entity_offset $offset
    } elseif {$type == "path"} {
        set entity_offset [expr {$offset + 8*$NVM_CFG_PATH_OFFSET + 8*$path*$NVM_CFG_PATH_SIZE}]        
    } elseif {$type == "port"} {        
        set entity_offset [expr {$offset + 8*$NVM_CFG_PORT_OFFSET + 8*$port*$NVM_CFG_PORT_SIZE}]        
    } elseif {$type == "func"} {
        set entity_offset [expr {$offset + 8*$NVM_CFG_FUNC_OFFSET + 8*$func*$NVM_CFG_FUNC_SIZE}]        
    } else {
        puts "Wrong type !"
        return 0
    }
    
    #Calculate offset in dwords
    set dword_offset [expr {$entity_offset / 32}]
    # binary file has 8 characters for each dword
    set char_offset [expr {$dword_offset*8}]
    set fmt "H$char_offset"
    #read a dword = 8 characters
    append fmt "H8"
    
    binary scan $nvm_config $fmt tmp val
    set value [format "0x%s" $val]
 
    set shift [expr {$entity_offset % 32}]
    set mask [expr {1 << $size}]
    set mask [expr {($mask - 1) << $shift}]
      
    if {$size < 32} {
	set value [expr {$value & $mask}]
    }

    set value [expr {$value >> $shift}]
    return $value
}

proc nvm_free_structs {{nvm_config} {shmem_config}} {
    
}

proc parse_nvm_cfg {args} {
    variable nvm_cfg_file
    
    if {[llength $args] < 4} {
	puts "Usage: parse_nvm_cfg <chip 57940/57980> <nvm cfg bin file> <nvm meta text file> <outfile>"
	return -1
    }

    set chip_num [lindex $args 0]
    if {($chip_num != "57940") && ($chip_num != "57980")} {
	puts "chip should be 57940 or 57980"
	return -1
    }
    set nvm_cfg_file [lindex $args 1]
    set nvm_meta_file [lindex $args 2]
    set outfile [lindex $args 3]
    
    set ::current(VIRTUAL_NVM_MODE) 1
    set ::current(DEV) 1
    set ::current(FUNC) 0
    set ::current(TOTAL_DEV) 16
    set ::sys(NO_PCI) 0
    set ::current(SPECIAL_NVM_MODE_NUM_PORTS) 2
    set ::current(SPECIAL_NVM_MODE_FUNC_NUM) 0
    set ::current(VERIFY_CFG) 0
		
    if {$chip_num == 57940} {
	set ::current(DID) 0x8070
	set ::current(CHIP_NUM) 57940
    } else {
	set ::current(DID) 0x1634
	set ::current(CHIP_NUM) 57980
    }      
	
    source $nvm_meta_file
    	
    puts "Parse $nvm_cfg_file to $outfile"
    nvmcfgT::nvm_cfg -dump $outfile    
    return 0
}
