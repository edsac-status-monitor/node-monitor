# Node Monitor
This project is part of the work on a status monitor for the EDSAC machine. The software which runs on measurement nodes taking sampling data and finding faults in the data which can be networked to the mothership. It is designed to be used with a Raspberry Pi.

## Dependencies
This project requires three libraries:
* [libedsacnetworking][libedsacnetworking]
* [libxml2][libxml2]
* [wiringPi][wiringpi]

[libedsacnetworking]: https://github.com/edsac-status-monitor/libedsacnetworking
[libxml2]: http://xmlsoft.org/
[wiringpi]: http://wiringpi.com/

## Function
The sampling node is connected donwstream to sampling hardware connected to an EDSAC chassis and upstream to the mothership via an ethernet network. The program functions by configuring the sampling hardware, establishing a network connection to the mothership and then looping through reading in sample data, analysing it and sending any fault messages to the mothership. The program is configured by a set of files which can be specified at runtime.

The program also has runtime options to only parse configuration files, choose to get sampled data from a csv file for testing or from connected hardware and to print error messages to the screen instead of sending them to the mothership.

## Usage
```
Usage: edsac-status-monitor [options]
Options:
  --config-dir <directory>       The directory in which to look for configuration files
  --chassis-file <filename>      The filename of the chassis configuration file within the configuration directory
  --wiring-file <filename>       The filename of the wiring configuration file within the configuration directory
  --calibration-file <filename>  The filename of the calibration configuration file within the configuration directory
  --tx-addr <address>            The IP address of the mothership
  --tx-port <port>               The IP port upon which to communicate with the mothership with
  --no-up-network                Do not relay any error messages to the mothership and simply echo them
  --help                         Display this help message
  --read-config                  Echo the parsed contents of the configuration files
  --test-sample-file <filename>  The filename of a csv file containing sample data to use instead of sampling from GPIO pins
```
## Configuration Files
Configurations files describe how the node is setup - what it is connected to and how. All configuration files are in the form of XML files and a description of the contents of each follows.
### Chassis Model Nodes
* ``<circuit>`` The root node
  #### Contains
  One or more:
  * ``<tp>``
  
* ``<tp>`` A testpoint in the chassis. If empty it is considered an input to the chassis otherwise, child nodes describe the logical connection to other test points.
  #### Attributes
  * ``min`` A float representing the minimum voltage which can be considered a logic high. Required.
  * ``max`` A float representing the maximum voltage which can be considerd a logic high. Required.
  * ``id`` An id used to represent this test point in this and other files. Required. Must be unique.
  #### Contains
  Zero or one:
  * ``<and>``
  * ``<or>``
  * ``<not>``
  * ``<ref>``
  
* ``<and>`` Represents a logical AND of the child nodes.
  #### Attributes
  * ``valve_no`` An integer which describes the valve driving this and gate.
  #### Contains
  Two or more:
  * ``<and>``
  * ``<or>``
  * ``<not>``
  * ``<ref>``
  
* ``<or>`` Represents a logical OR of the child nodes.
  #### Attributes
  * ``valve_no`` An integer which describes the valve driving this or gate.
  #### Contains
  Two or more:
  * ``<and>``
  * ``<or>``
  * ``<not>``
  * ``<ref>``
  
* ``<not>`` Represents a logical AND of the child nodes.
  #### Attributes
  * ``valve_no`` An integer which describes the valve driving this not gate.
  #### Contains
  Exactly one:
  * ``<and>``
  * ``<or>``
  * ``<not>``
  * ``<ref>``
  
* ``<ref>`` Refernces a ``<tp>`` as an input to a logical gate. 
  #### Contains
  The test point's id.
  
### Wiring Nodes
* ``<wiring>`` The root node. There must be a ``tp`` for every ``tp`` in the model file.
  #### Attributes
  * ``hold-pin`` The physical pin number used for the HOLD signal to the sampling hardware.
  #### Contains
  One or more:
  * ``<tp>``
  
* ``<tp>`` A test point. Corresponds to a test point in the chassis file.
  #### Attributes
  * ``id`` The id corresponding to an id in the model file.
  * ``pin`` The physical pin number that this test point is wired to.
  * ``resistor`` A string in the format NS where N is the chip index of the potentiometer used for this channel and S is either A or B corresponding to the first or second potentiometer on that chip.
  * ``attenuation`` A float describing the attenuation this channel of the sampling hardware has been set to. A number smaller than one relates to a gain.
  
### Calibration
* ``<calibration>`` The root node. There must be a ``tp`` for every ``tp`` in the model file.
  #### Contains
  One or more:
  * ``<tp>``
  
* ``<tp>`` A test point. Corresponds to a test point in the chassis file.
  #### Attributes
  * ``id`` The id corresponding to an id in the model file.
  * ``threshold`` A float describing the exact threshold desired for this test point. This value must lie within the the ``min`` and ``max`` attributes specified on the corresponding test point in the model file.
