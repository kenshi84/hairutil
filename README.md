# hairutil
A command-line tool for handling hair files:
- .bin
- .hair
- .data
- .ply
- .ma

```
$ hairutil --help
  hairutil COMMAND {OPTIONS}

  OPTIONS:

      Commands:
        convert                           Convert file type
        subsample                         Subsample strands

      Common options:
        -i[PATH], --input-file=[PATH]     (*)Input file
        -o[EXT], --output-ext=[EXT]       (*)Output file extension
        --overwrite                       Overwrite when output file exists
        --ply-load-default-nsegs=[N]      Default number of segments per strand
                                          for PLY files [0]
        --ply-save-binary                 Save PLY files in binary format
        -v[NAME], --verbosity=[NAME]      Verbosity level name
                                          {trace,debug,info,warn,error,critical,off}
                                          [info]
        -h, --help                        Show this help message
```

## Compiling
```
git clone --recursive https://github.com/kenshi84/hairutil
mkdir build && cd build
cmake ..
make -j
```

## Usage

### `convert` command
```
hairutil convert --input-file ~/CT2Hair/output/Bangs.bin --output-ext ma
# Output saved to ~/CT2Hair/output/Bangs.ma
```

### `subsample` command
```
$ hairutil subsample --help
  hairutil subsample {OPTIONS}

    Subsample strands

  OPTIONS:

      --target-count=[N]                (*)Target number of hair strands
      --scale-factor=[R]                Factor for scaling down the Poisson disk
                                        radius [0.9]
```

Example:
```
hairutil subsample --input-file ~/CT2Hair/output/Bangs.bin --output-ext bin --target-count 1000
# Output saved to ~/CT2Hair/output/Bangs_1000.bin
```
