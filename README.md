# SMR-Simulator
A SMR simulator

# Parameters
```
-c: cvsFlag
-s: Disk init size (GiB)
-i: Filename
```

# CMR
``` js
make cmr
./bin/cmr -c -s 100 -i ../Trace/TPCC.csv
```

# Native A
``` js
make native_a
./bin/native_a -c -s 100 -i ../Trace/TPCC.csv
```

# Native B
``` js
make native_b
./bin/native_b -c -s 100 -i ../Trace/TPCC.csv
```

# FluidSMR
``` js
make fluid_smr
./bin/fluid_smr  -c -s 100 -i ../Trace/TPCC_Hint.csv
```

# Hybrid
``` js
make hybrid
./bin/hybrid -c -s 100 -i ../Trace/TPCC.csv
```