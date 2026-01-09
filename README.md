

# 0.functions

* dump es
* print pts
* show info

# 1. compile

```shell
g++ TsParser.cpp main.cpp -o tsParser
# if run some erros, compile like this:
g++ TsParser.cpp main.cpp -o tsParser -static-libgcc -static-libstdc++
```

# 2. usage

```
Usage: ./tsParser <infile> [OPTIONS...]
OPTIONS:
  -i, --infile <FILE>     Input TS file path
  -s, --showinfo          Show stream information
  -o, --output_pid [PID]  Output PID to out_pid.es (no PID => dump all PIDs)
  -p, --print [PID]       Print pts (no PID => print all PIDs)
  -h, --help              Show this help message
  -v, --version           Show version information

Example: ./tsParser -i input.ts -p

If only <infile> is provided, it is equivalent to: ./tsParser -i <infile> -s
```

