# 
# Usage: To re-create this platform project launch xsct with below options.
# xsct C:\Users\il0ve\CMPE_240\BREAKOUT\vitis\breakout_wrapper\platform.tcl
# 
# OR launch xsct and run below command.
# source C:\Users\il0ve\CMPE_240\BREAKOUT\vitis\breakout_wrapper\platform.tcl
# 
# To create the platform in a different location, modify the -out option of "platform create" command.
# -out option specifies the output directory of the platform project.

platform create -name {breakout_wrapper}\
-hw {C:\Users\il0ve\CMPE_240\BREAKOUT\external\design_1_wrapper.xsa}\
-out {C:/Users/il0ve/CMPE_240/BREAKOUT/vitis}

platform write
domain create -name {standalone_ps7_cortexa9_0} -display-name {standalone_ps7_cortexa9_0} -os {standalone} -proc {ps7_cortexa9_0} -runtime {cpp} -arch {32-bit} -support-app {hello_world}
platform generate -domains 
platform active {breakout_wrapper}
domain active {zynq_fsbl}
domain active {standalone_ps7_cortexa9_0}
platform generate -quick
platform generate
platform active {breakout_wrapper}
platform config -updatehw {C:/Users/il0ve/CMPE_240/BREAKOUT/external/design_1_wrapper_v2.xsa}
platform generate -domains 
