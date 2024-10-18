# PhreakScript
A utility to automate the installation, maintenance, and debugging of Asterisk/DAHDI, while integrating additional patches to provide the richest telephony experience.

PhreakScript installs:

- Asterisk 21.4.3 (latest standard release[1] of Asterisk)
- DAHDI Linux 3.4.0 (with optional DAHDI install flag)
- DAHDI Tools 3.4.0 (with optional DAHDI install flag)
- wanpipe 7.0.38 (with optional wanpipe install flag)
- many additional features and stability improvements
   - Restores the "great purge" of DAHDI drivers that were removed in 2018 by Sangoma
   - DAHDI/wanpipe/LibPRI compilation fixes
   - Native coin detection and blue boxing support
   - Class 4 and Class 5 coin trunk support
   - Real time dial pulsing support
   - "Hearpulsing" patches
   - Automatic dialplan context to digit map generation
   - Broadworks compatible device feature key synchronization (PJSIP)
   - Broadworks compatible Shared Call Appearances (PJSIP)
   - Presence publishing (PJSIP)
   - Message Send Protocol send support
   - SMDR support for "WHOZZ Calling?" call accounting devices
   - AGI `RECORD FILE` option to require noise before silence detection
   - Optional build enhancements
      - `chan_sccp` (improved community Skinny/SCCP channel driver), with compilation fixes
      - Cisco Call Manager support for `chan_sip`
      - Restored and enhanced [`chan_sip`](https://github.com/InterLinked1/chan_sip) for master / versions 21+ (removed by Sangoma)
   - Adds the following applications:
      - ``Assert``
      - ``ReturnIf``
      - ``SendMail``
      - ``MallocTrim``
      - ``PartialPlayback``
      - ``LoopPlayback``
      - ``RandomPlayback``
      - ``SayTelephoneNumber``
      - ``Audichron``
      - ``FeatureProcess``
      - ``SelectiveFeature``
      - ``RemoteAccess``
      - ``George``
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
      - ``Signal``
      - ``WaitForSignal``
      - ``WaitForDeposit``
      - ``CoinDisposition``
      - ``LocalCoinDisposition``
      - ``CoinCall``
      - ``ACTS``
      - ``SendCWCID``
      - ``DAHDIMonitor``
      - ``DialSpeedTest``
      - ``LoopDisconnect``
      - ``ToneSweep``
      - ``DialTone``
      - ``InbandDial``
      - ``Verify``
      - ``OutVerify``
      - ``KeyPrefetch``
      - ``PhreakNetDial``
      - ``SIPAddParameter``
      - ``IRCSendMessage``
      - ``AlarmSensor``
      - ``AlarmKeypad``
      - ``AlarmEventReceiver`` (not to be confused with ``AlarmReceiver``)
      - ``Softmodem`` (third-party, with compiler fixes and enhancements, including TDD modem)
      - ``TddRx``, ``TddTx`` (third-party)
   - Adds the following functions:
      - ``TECH_EXISTS``
      - ``DTMF_FLASH``
      - ``DTMF_TRACE``
      - ``NUM2DEVICE``
      - ``TEXT_QUERY``
      - ``COIN_DETECT``
      - ``COIN_EIS``
      - ``PHREAKNET``
      - ``NOTCH_FILTER``
      - ``DB_CHANNEL``
      - ``DB_CHANNEL_PRUNE``
      - ``DB_CHANNEL_PRUNE_TIME``
      - ``DB_MAXKEY``
      - ``DB_MINKEY``
      - ``DB_UNIQUE``
      - ``NANPA``
      - ``SIP_PARAMETER``
      - ``MESSAGE_INTERCEPT_SUB``
      - ``GROUP_VAR`` (third-party)
      - ``GROUP_MATCH_LIST_START`` (third-party)
   - Miscellaneous improvements
      - Enhances performance by completely removing Newexten AMI event
      - Adds fax timing and parameter control to `chan_sip`
      - Adds prefix capabilities to `include => `
      - Fixes ulaw/gsm codec translation bug

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

[1] Normally, PhreakScript installs the latest LTS version of Asterisk. However, version 21 will be installed by default until version 22 is released,
    due to the significant change in functionality from version 20 which allows for a richer installation by default.
    To install the latest LTS version (20), you can specify the version explicitly e.g. `--version=20.5.0`.
    However, there should not be a need to do so unless you are using obsolete modules that were removed in Asterisk 21.

### Installation

Please refer to the Docs: https://docs.phreaknet.org/#phreakscript

In a nutshell, run:

**Linux:**

```cd /usr/local/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

**FreeBSD:**

```pkg install -y wget && cd /usr/local/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

Then, you can use PhreakScript. Run ```phreaknet help``` or ```phreaknet examples``` to get started.

For a basic install, you can run `phreaknet install`

To install with DAHDI, run `phreaknet install --dahdi` (add `--drivers` to also restore drivers that were removed upstream in DAHDI 3).

For a guided, interactive installation, you can also run `phreaknet wizard`. The wizard will determine what installation options are best for you, based on your preferences.

PhreakScript must be run as root, even if Asterisk will not be installed to run as root.

PhreakScript is primarily supported on Debian-based Linux systems, and DAHDI and Asterisk are best supported on these platforms.
Limited support is available for other Linux distros (Fedora, RHEL, Rocky Linux, SUSE, Arch Linux, etc.).
Extremely limited support exists for FreeBSD, and BSDs (and UNIX in general) are not recommended for running Asterisk/DAHDI - use Linux instead if possible.

### License

PhreakScript itself is licensed under the Apache 2.0 License. This includes any scripts in this repository.

However, any Asterisk modules ("C" code) in this repository (such as those that may be installed by PhreakScript) are licensed under the GNU General Public License Version 2 (GPLv2), per the Asterisk licensing terms.

If you have copyright or licensing doubts, please refer to any copyrights and licensing terms in individual source files.

### Documentation

Please refer to the PhreakNet Asterisk documentation for PhreakScript-specific module documentation: https://asterisk.phreaknet.org/

### Change Log

Please run ```phreaknet about```

### Reporting Issues

The preferred issue reporting procedure is by cutting us a ticket at InterLinked Issues: https://issues.interlinked.us/

Choose "PhreakScript" as the category.

### Pull Requests

Please see "Contributing to PhreakScript" in the Docs: https://docs.phreaknet.org/#contributions
