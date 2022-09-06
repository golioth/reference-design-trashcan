Golioth Trashcan Monitor Reference Design
##################

Overview
********

This is a Reference Design for remote trashcan monitoring. It is targeted at municpalities that would like to monitor a variety of trash recipticals and report back on a programmable cadence. The device would be battery powered, so the goal is to stretch the lifetime of batteries as long as possible. 

The device captures and sends the following readings back to the Golioth Cloud, which are then displayed on a dashboard:

- Distance sensor - Used to measure trash height

  - Distance (m)
  - Proximity (t/f)
  - Level (range of values)
- Weather Sensor - Used to monitor surrounding conditions

  - Temperature
  - Pressure
  - Humidity
- Gas Sensor - Used to monitor trash contents

  - CO2
  - VOC
- Accelerometer - Used to measure trash can orientation (tipped over or not)

  - X
  - Y 
  - Z

Possible reuse 
==============

At a higher level, this Reference Design is a cellular based sensor platform that transmits data on an occasional basis. The Trashcan Reference Design uses a laser distance sensor pointed down into a trashcan to monitor whether you should send a truck or worker to empty it. However, the same sensor could easily be repurposed for:

- Liquid level monitoring
- Job site maintenance
- Proximity detection (security)

Given the flexibility of the Zephyr RTOS and Golioth Zephyr SDK it is built upon, users should be able to swap out sensors and report different values to the cloud with minimal effort

There are additional sensors for environmental monitoring featured on this Reference Design, but they can be removed with minimal effort as well, to save cost and power.

Requirements
************

- Golioth credentials
- SIM Card with data
- Hardware

  - Sparkfun Thing Plus nRF9160
  - Sparkfun Environmental Combo QWIIC board
  - Adafruit VL53L0x STEMMA board
  - External Lithium Polymer Battery, plugged into the 2 mm JST plugged
  - Enclosure (user discretion)
- External tools 

  - JLink or similar programmer (nRF9160-DK) to directly program the Sparkfun Thing Plus nRF9160 over the 10 pin (0.05in, 02x05 connector) SWD connector. If you choose not to use a programmer, you will need to load the MCUboot image over USB (not recommended).


Replicating the project setup
*******************************

Replicate the project. If you are using a Virtual Python instance, you will need to first activate your `.venv` instance. After `west update`, you will have listed two directories at the 

.. code-block:: console

   $ source .venv/bin/activate
   $ (.venv) mkdir golioth-reference-design-trashcan
   $ (.venv) cd golioth-reference-design-trashcan
   $ (.venv) west init -m git@github.com:golioth/reference-design-trashcan.git .
   $ (.venv) west update
   

Build Zephyr sample application for Sparkfun Thing Plus nRF9160 from the top level of your project. After a successful build you will see a new `build` directory. Note that any changes (and git commmits) to the project itself will be inside the `app` folder. The `build` and `deps` directories being one level higher prevents the repo from cataloging all of the changes to the dependencies and the build (so no .gitignor is needed)

During building Replace <your.semantic.version> to utilize the DFU functionality on this Reference Design.

.. code-block:: console

   $ (.venv) west build -b sparkfun_thing_plus_nrf9160_ns app -- -DCONFIG_MCUBOOT_IMAGE_VERSION=\"<your.semantic.version>\"
   $ (.venv) west flash

Configure PSK-ID and PSK using the device shell based on your Golioth credentials and reboot:

.. code-block:: console

   uart:~$ settings set golioth/psk-id <my-psk-id@my-project>
   uart:~$ settings set golioth/psk <my-psk>
   uart:~$ kernel reboot cold


Elements of Golioth used in this Reference Design
=================================================

- Over-the-air firmware update
- LightDB Stream
- Settings service
- Logging
