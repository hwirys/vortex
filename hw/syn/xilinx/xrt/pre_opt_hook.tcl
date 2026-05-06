if {[info exists ::env(TOOL_DIR)]} {
    set tool_dir $::env(TOOL_DIR)
} else {
    set tool_dir /home/sicdl/vortex/hw/scripts
    puts "WARNING: TOOL_DIR not set; falling back to $tool_dir"
}
source ${tool_dir}/xilinx_async_bram_patch.tcl

report_utilization -file hier_utilization.rpt -hierarchical -hierarchical_percentages