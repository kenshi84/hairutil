# hairutil
A command-line tool for handling hair files:
- .bin
- .hair
- .data
- .ply
- .ma
- .abc

```
$ hairutil --help
  hairutil COMMAND {OPTIONS}

  OPTIONS:

      Commands:
        convert                           Convert file type
        decompose                         Decompose into individual curves
        info                              Print information
        subsample                         Subsample strands

      Common options:
        -i[PATH], --input-file=[PATH]     (REQUIRED) Input file
        -o[EXT], --output-ext=[EXT]       Output file extension
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

### `decompose` command
```
hairutil decompose --input-file ~/CT2Hair/output/Bangs.bin --output-ext ply --confirm
# Output saved to ~/CT2Hair/output/Bangs_decomposed_ply/*.ply
```

### `info` command
```
hairutil info --input-file ~/cemyuksel/wCurly.hair
[info] Number of strands: 50000
[info] Number of points: 3441580
[info] ================================================================
[info] Segments array: Yes
[info] Points array: Yes
[info] Thickness array: No
[info] Transparency array: No
[info] Colors array: No
[info] Default thickness: 0.1
[info] Default transparency: 0.4
[info] Default color: (0.28718513, 0.22646429, 0.14465585)
[info] ================================================================
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
