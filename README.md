# hairutil
A command-line tool for handling hair files.

Supported file formats:
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
        filter                            Extract strands that pass given filter
        findpenet                         Find penetration against head mesh
        getcurvature                      Get discrete curvature & torsion
        info                              Print information
        resample                          Resample strands s.t. every segment is
                                          shorter than twice the target segment
                                          length
        smooth                            Smooth strands
        stats                             Generate statistics
        subsample                         Subsample strands
        transform                         Transform strand points

      Common options:
        -i[PATH], --input-file=[PATH]     (REQUIRED) Input file
        -o[EXT], --output-ext=[EXT]       Output file extension
        --overwrite                       Overwrite when output file exists
        --ply-load-default-nsegs=[N]      Default number of segments per strand
                                          for PLY files [0]
        --ply-save-ascii                  Save PLY files in ASCII format
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

### `filter` command
```
$ hairutil filter --help
  hairutil filter {OPTIONS}

    Extract strands that pass given filter

  OPTIONS:

      -k[KEY], --key=[KEY]              Filtering key chosen from:
                                        length (Total length)
                                        nsegs (Number of segments)
                                        tasum (Turning angle sum)
                                        maxseglength (Maximum of segment length)
                                        minseglength (Minimum of segment length)
                                        maxptcrr (Maximum of point circumradius
                                        reciprocal)
                                        minptcrr (Minimum of point circumradius
                                        reciprocal)
                                        maxptta (Maximum of point turning angle)
                                        minptta (Minimum of point turning angle)
      --lt=[R]                          Less-than threshold
      --gt=[R]                          Greater-than threshold
      --leq=[R]                         Less-than or equal-to threshold
      --geq=[R]                         Greater-than or equal-to threshold
```

Example:
```
hairutil filter -i ~/CT2Hair/output/Bangs.bin -o ply --overwrite --key length --geq 174.96289
# Output saved to ~/CT2Hair/output/Bangs_filtered_length_geq_174.96289.ply
```

### `findpenet` command
```
$ hairutil findpenet --help
  hairutil findpenet {OPTIONS}

    Find penetration against head mesh

  OPTIONS:

      -m[PATH], --mesh-path=[PATH]      (REQUIRED) Path to triangle mesh
      -d[RATIO],
      --decimate-ratio=[RATIO]          Ratio for decimating triangle mesh
                                        [0.25]
      -t[RATIO],
      --threshold-ratio=[RATIO]         Threshold ratio [0.3]; detect strand as
                                        penetrating if #in-points is more than
                                        this value times #total-points
      --no-export                       Do not export result to txt
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

### `resample` command
```
$ hairutil resample --help
  hairutil resample {OPTIONS}

    Resample strands s.t. every segment is shorter than twice the target segment
    length

  OPTIONS:

      --target-segment-length=[R]       Target segment length [2.0]
      --min-num-segments=[N]            Minimum number of segments per hair [20]
```

### `stats` command
```
$ hairutil stats --help
  hairutil stats {OPTIONS}

    Generate statistics

  OPTIONS:

      --sort-size=[N]                   Print top-N sorted list of items [10]
      --no-export                       Do not export result to a .xlsx file
      --export-raw-strand               Include raw strand data in exported file
      --export-raw-segment              Include raw segment data in exported
                                        file
      --export-raw-point                Include raw point data in exported file
      --no-print                        Do not print the stats
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
      --indices=[N,...]                 Comma-separated list of strand indices
                                        to extract, or a path to .txt file
                                        containing such a list
      --exclude                         Exclude the specified strands instead of
                                        including them
```

Example:
```
hairutil subsample --input-file ~/CT2Hair/output/Bangs.bin --output-ext bin --target-count 1000
# Output saved to ~/CT2Hair/output/Bangs_1000.bin
```

```
hairutil subsample -i test/data/Bangs_100.bin -o ply --indices 65,32,4,36,0
# Output saved to test/data/Bangs_100_indices_0_4_32_36_65.ply
```

### `transform` command
```
$ hairutil transform --help
  hairutil transform {OPTIONS}

    Transform strand points

  OPTIONS:

      -s[R], --scale=[R]                Uniform scaling factor [1.0]
      -t[R,R,R], --translate=[R,R,R]    Comma-separated 3D vector for
                                        translation [0,0,0]
      -R[R,R,R,R,R,R,R,R,R],
      --rotate=[R,R,R,R,R,R,R,R,R]      Comma-separated row-major 3x3 matrix for
                                        rotation [1,0,0,0,1,0,0,0,1]
```
Example:
```
hairutil transform -i test/data/Bangs_100.bin -o ply --overwrite --scale 1.2 --translate 12.3,45.6,78.9 --rotate 0.407903582,-0.656201959,0.634833455,0.838385462,0.54454118,0.0241773129,-0.361558199,0
.52237314,0.77227056
# Output saved to test/data/Bangs_100_tfm_s_1.2_t_12.3_45.6_78.9_R_0.407904_-0.656202_0.634833_0.838385_0.544541_0.0241773_-0.361558_0.522373_0.772271.ply
```
