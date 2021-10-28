# Instructions and examples for measuring power with PowerMon

## Last Updated, 8/5/15, by Jeff Young

**Note**: Ideally use a different system to host the USB connection to reduce interference with
local sampling.

### To run the PowerMon application:

1. First make sure that the `/dev/ttyUSB*` device shows up under your system. If not, it is likely not connected correctly or powered correctly.

2. Execute the program:

`<program_exe> <port_dev> <mask> <sample_pd> <num_samples>`

Valid parameters are:
<port_dev>:  usually /dev/ttyUSB0 or /dev/ttyUSB1 but depends on how many USB devices are installed
<mask>: 0-128 (0b0 - 0b10000000); specifies which sensor to read from. Note that ATX uses sensors 0-2.
<sample_pd>: 1-1000; measure at a rate between 1 and 1KHz
<num_samples>: 1-[INT_MAX]; specify the total number of samples to measure

3. Parse the data
To calculate total time taken for a specific sample rate and number of samples: <num_samples> / <sample_pd>

### Examples
  
Ex 1: Sample sensor 1 (0b10) at 1 Hz with 4 samples.
  
`$./run_powermon /dev/ttyUSB0 2 1 4`

Ex 2: Sample sensor 6 (0b100000) at 1 KHz with 4000 samples (~4 seconds).

`$./run_powermon /dev/ttyUSB0 64 1000 4000`

Ex 3: Sample sensors 1,2, and 3 (0b1110) at 500 Hz with 200 samples (~400ms)

`$./run_powermon /dev/ttyUSB0 14 500 200`
