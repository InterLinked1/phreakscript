# PhreakScript
A utility to automate the installation, maintenance, and debugging of Asterisk/DAHDI, while integrating additional patches to provide the richest telephony experience.

PhreakScript installs:

- Asterisk 18.8 (latest LTS release of Asterisk)
- next branch of DAHDI Linux (newer than 3.1.0, with optional DAHDI install flag)
- DAHDI Tools 3.1.0 (with optional DAHDI install flag)
- many additional features and stability improvements (see change log for full details)
   - Restores the "great purge" of DAHDI drivers that were removed in 2018 by Sangoma
   - Fixes ulaw/gsm codec translation bug
   - Fixes infinite loop Dial bug
   - Adds the following applications:
      - ``Assert``
      - ``ReturnIf``
      - ``If``, ``EndIf``, ``ExecIf``
      - ``SendMail``
      - ``MallocTrim``
      - ``SendFrame``
      - ``StreamSilence``
      - ``ToneSweep``
      - ``ToneScan``
      - ``DialTone``
      - ``SIPAddParameter``
      - ``Softmodem`` (third-party)
      - ``TddRx``, ``TddTx`` (third-party)
   - Adds the following functions:
      - ``OTHER_CHANNEL``
      - ``NOTCH_FILTER``
      - ``EVAL_EXTEN``
      - ``DB_CHANNEL``
      - ``DB_CHANNEL_PRUNE``
      - ``JSON_DECODE``
      - ``SIP_PARAMETER``

PhreakScript is also useful for:
- automating installation and maintenance of Asterisk, Asterisk Test Suite, Asterisk Test Framework, DAHDI Linux, DAHDI Tools, and related resources
- validating Asterisk configuration
   - can find common syntax errors in dialplan code
   - can find missing audio files referenced by the ``Playback``, ``BackGround``, and ``Read`` applications
   - suggests optimizations that can be made to dialplan code to make it more readable and efficient
- debugging Asterisk configuration
- automating PhreakNet boilerplate dialplan installation

### Installation

Please refer to the Docs: https://docs.phreaknet.org/#phreakscript

In a nutshell, run:

```wget https://docs.phreaknet.org/script/phreaknet.sh; chmod +x phreaknet.sh; ./phreaknet.sh make; exec $SHELL```

Then, you can use PhreakScript. Run ```phreaknet help``` or ```phreaknet examples``` to get started.

PhreakScript must be run as root, even if Asterisk does not run as root.

PhreakScript is geared towards Debian-based systems, but pull requests to add support for other Linux distros or BSD are welcome.

### Change Log

Please run ```phreaknet about```

### Pull Requests

Please see "Contributing to PhreakScript" in the Docs: https://docs.phreaknet.org/#contributions
