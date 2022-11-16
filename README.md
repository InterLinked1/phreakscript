# PhreakScript
A utility to automate the installation, maintenance, and debugging of Asterisk/DAHDI, while integrating additional patches to provide the richest telephony experience.

PhreakScript installs:

- Asterisk 20.0.0 (latest LTS release of Asterisk)
- DAHDI Linux 3.2.0 (with optional DAHDI install flag)
- DAHDI Tools 3.2.0 (with optional DAHDI install flag)
- many additional features and stability improvements
   - Restores the "great purge" of DAHDI drivers that were removed in 2018 by Sangoma
   - DAHDI/wanpipe/LibPRI compilation fixes
   - Native coin and blue boxing support
   - Real time dial pulsing support
   - Automatic dialplan context to digit map generation
   - Presence publishing (PJSIP)
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
      - ``RemoteAccess``
      - ``RequestCallback``
      - ``CancelCallback``
      - ``ScheduleWakeupCall``
      - ``HandleWakeupCall``
      - ``CCSA``
      - ``PreDial``
      - ``SetMWI``
      - ``PlayDigits``
      - ``StreamSilence``
      - ``RevertivePulse``
      - ``SendFrame``
      - ``WaitForFrame``
      - ``Broadcast``
      - ``Signal``
      - ``WaitForSignal``
      - ``WaitForDeposit``
      - ``CoinDisposition``
      - ``SendCWCID``
      - ``DialSpeedTest``
      - ``LoopDisconnect``
      - ``ToneSweep``
      - ``DialTone``
      - ``Verify``
      - ``OutVerify``
      - ``KeyPrefetch``
      - ``SIPAddParameter``
      - ``IRCSendMessage``
      - ``Softmodem`` (third-party, with compiler fixes)
      - ``TddRx``, ``TddTx`` (third-party)
   - Adds the following functions:
      - ``TECH_EXISTS``
      - ``DTMF_FLASH``
      - ``NUM2DEVICE``
      - ``TEXT_QUERY``
      - ``COIN_DETECT``
      - ``COIN_EIS``
      - ``EXPORT``
      - ``NOTCH_FILTER``
      - ``DB_CHANNEL``
      - ``DB_CHANNEL_PRUNE``
      - ``DB_CHANNEL_PRUNE_TIME``
      - ``DB_MAXKEY``
      - ``DB_MINKEY``
      - ``DB_UNIQUE``
      - ``NANPA``
      - ``SIP_PARAMETER``
      - ``GROUP_VAR`` (third-party)
      - ``GROUP_MATCH_LIST_START`` (third-party)
   - Miscellaneous improvements
      - Enhances performance by completely removing Newexten AMI event
      - Adds fax timing and parameter control to `chan_sip`
      - Adds prefix capabilities to `include => `
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

For a basic install, you can run `phreaknet install`

To install with DAHDI, run `phreaknet install --dahdi`

For a guided, interactive installation, you can also run `phreaknet wizard`. The wizard will determine what installation options are best for you, based on your preferences.

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
