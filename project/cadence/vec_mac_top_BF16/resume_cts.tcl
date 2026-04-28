# Restore placed design
set base "$env(HOME)/Documents/ECE510/project/510Project/project/cadence/vec_mac_top_BF16"
restoreDesign $base/outputs/vec_mac_top_placed.enc.dat vec_mac_top

routeDesign
file mkdir reports
reportPower > reports/power_final.rpt
saveNetlist  $base/outputs/vec_mac_top_final.v
saveDesign   $base/outputs/vec_mac_top_final.enc
