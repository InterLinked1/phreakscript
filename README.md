# PhreakScript
A utility to automate the installation, maintenance, and debugging of Asterisk/DAHDI, while integrating additional patches to provide the richest telephony experience.

PhreakScript installs:

- Asterisk 18.10.1 (latest LTS release of Asterisk)
- next branch of DAHDI Linux (newer than 3.1.0, with optional DAHDI install flag)
- DAHDI Tools 3.1.0 (with optional DAHDI install flag)
- many additional features and stability improvements (see change log for full details)
   - Restores the "great purge" of DAHDI drivers that were removed in 2018 by Sangoma
   - Optional build enhancements
      - `chan_sccp` (improved community Skinny/SCCP channel driver)
      - Cisco Call Manager support for `chan_sip`
   - Adds the following applications:
      - ``Assert``
      - ``ReturnIf``
      - ``If``, ``Else``, ``ElseIf``, ``EndIf``, ``ExitIf``
      - ``SendMail``
      - ``MallocTrim``
      - ``LoopPlayback``
      - ``RandomPlayback``
      - ``SayTelephoneNumber``
      - ``FeatureProcess``
      - ``SelectiveFeature``
      - ``PlayDigits``
      - ``StreamSilence``
      - ``RevertivePulse``
      - ``Signal``
      - ``WaitForSignal``
      - ``WaitForDeposit``
      - ``CoinDisposition``
      - ``ToneSweep``
      - ``DialTone``
      - ``Verify``
      - ``OutVerify``
      - ``KeyPrefetch``
      - ``SIPAddParameter``
      - ``Softmodem`` (third-party)
      - ``TddRx``, ``TddTx`` (third-party)
   - Adds the following functions:
      - ``TECH_EXISTS``
      - ``NUM2DEVICE``
      - ``COIN_DETECT``
      - ``OTHER_CHANNEL``
      - ``NOTCH_FILTER``
      - ``EVAL_EXTEN``
      - ``DB_CHANNEL``
      - ``DB_CHANNEL_PRUNE``
      - ``DB_CHANNEL_PRUNE_TIME``
      - ``DB_MAXKEY``
      - ``DB_MINKEY``
      - ``DB_UNIQUE``
      - ``SIP_PARAMETER``
   - Miscellaneous improvements
      - Enhances performance by disabling Newexten AMI event
      - Adds fax timing and parameter control to `chan_sip`
      - Fixes ulaw/gsm codec translation bug
      - Fixes infinite loop Dial bug

PhreakScript is also useful for:
- automating installation and maintenance of Asterisk, Asterisk Test Suite, Asterisk Test Framework, DAHDI Linux, DAHDI Tools, and related resources
- validating Asterisk configuration
   - can find common syntax errors in dialplan code
   - can find missing audio files referenced by the ``Playback``, ``BackGround``, and ``Read`` applications
   - suggests optimizations that can be made to dialplan code to make it more readable and efficient
- generating Asterisk user documentation
- debugging Asterisk configuration
- generating core dumps
- automating PhreakNet boilerplate dialplan installation

### Installation

Please refer to the Docs: https://docs.phreaknet.org/#phreakscript

In a nutshell, run:

**Linux:**

```cd /usr/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

**FreeBSD:**

```pkg install -y wget && cd /usr/local/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

Then, you can use PhreakScript. Run ```phreaknet help``` or ```phreaknet examples``` to get started.

PhreakScript must be run as root, even if Asterisk does not run as root.

PhreakScript is primarily supported on Debian-based Linux systems. Support has also been added for FreeBSD. Pull requests to add support for other Linux distros or BSD are welcome.

### Documentation

Please refer to the PhreakNet Asterisk documentation for PhreakScript-specific module documentation: https://asterisk.phreaknet.org/

### Change Log

Please run ```phreaknet about```

### Reporting Issues

The preferred issue reporting procedure is by cutting us a ticket at InterLinked Issues: https://issues.interlinked.us/

Choose "PhreakScript" as the category.

### Pull Requests

Please see "Contributing to PhreakScript" in the Docs: https://docs.phreaknet.org/#contributions
