

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
  -h | --help           : Show this help message
  -i | --infile         : Input TS file path
  -v | --video_pid      : Video PID in decimal or hex (prefix 0x)
  -a | --audio_pid      : Audio PID in decimal or hex (prefix 0x)
  -t | --text_pid       : Text PID in decimal or hex (prefix 0x)
  -d | --dump           : Dump all PIDs to es files
  -s | --showinfo       : Show stream information
  -o | --output_pid     : Output PID to out_pid.es
  -p | --print          : Print pts

Example: ./tsParser -i input.ts -v 0x100 -a 0x101

If only <infile> is provided, it is equivalent to: ./tsParser -i <infile> -s
```

