# PhreakScript

**A utility to automate the installation, maintenance, and debugging of Asterisk/DAHDI, while integrating additional patches to provide the richest telephony experience.**

PhreakScript installs:

- Asterisk 22.5.0 (latest LTS release[1] of Asterisk)
- DAHDI Linux 3.4.0 (with optional DAHDI install flag)
- DAHDI Tools 3.4.0 (with optional DAHDI install flag)
- wanpipe 7.0.38 (with optional wanpipe install flag)
- Bug fixes and stability improvements
   - Restores the "great purge" of DAHDI drivers that were removed in 2018 by Sangoma (either by default, or with `--drivers` flag)
   - Numerous DAHDI/wanpipe/LibPRI compilation fixes
   - Bug fixes for reading and writing configuration file templates
   - Bug fix for ConfBridge shutdown race condition
   - Enhances performance by completely removing Newexten AMI event
   - Prevents duplicate Asterisk process creation
- Many additional features and improvements
   - Adds prefix capabilities to `include => `
   - Signaling enhancements
      - Native coin detection and blue boxing support
      - Class 4 and Class 5 coin trunk support
   - Analog enhancements
      - Real time dial pulsing support
      - "Hearpulsing" patches
      - SMDR support for "WHOZZ Calling?" call accounting devices
      - Supervision test and hook state applications
      - 1A2 key system music on hold support
   - SIP enhancements
      - Automatic dialplan context to digit map generation (`res_digitmap`)
	  - Broadworks compatible device feature key synchronization (PJSIP) (with `--experimental` flag)
	  - Broadworks compatible Shared Call Appearances (PJSIP) (with `--experimental` flag)
	  - Presence publishing (PJSIP)
	  - Restored and enhanced [`chan_sip`](https://github.com/InterLinked1/chan_sip) for master / versions 21+ (removed by Sangoma) (with `--sip` flag)
	     - Adds fax timing and parameter control to `chan_sip`
	     - Cisco Call Manager support for `chan_sip` (with `--cisco` flag)
	     - `chan_sccp` (improved community Skinny/SCCP channel driver), with compilation fixes (with `--sccp` flag)
   - Message Send Protocol send support
   - AGI `RECORD FILE` option to require noise before silence detection
   - Build system support for ALSA-dependent modules
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
      - ``OnHook``
      - ``OffHook``
      - ``SupervisionTest``
      - ``SendFrame``
      - ``WaitForFrame``
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
	  - ``SendFSK``, ``ReceiveFSK`` (third-party)
   - Adds the following functions:
      - ``TECH_EXISTS``
      - ``DTMF_FLASH``
      - ``DTMF_TRACE``
      - ``1A2_LINE_STATE``
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

> [!NOTE]
> [1] Currently, PhreakScript installs the latest LTS version of Asterisk. However,
    when a standard release is the most recent release, it may be installed by default.
    To install a specific major version, you can specify it explicitly e.g. `--version=22`.

> [!NOTE]
> PhreakScript is primarily an automation tool, but it also adds a significant amount of functionality to Asterisk beyond what is available upstream, making it a superset of "vanilla Asterisk". We aim to upstream our patches whenever practical; in fact, we are among the most active and frequent contributors to the Asterisk and DAHDI projects. This repository includes enhancements that are not available upstream as well as those that may be in the future but are not currently. A list of upstreamed Asterisk patches can be found at https://phreaknet.org/changes

### Installation

In a nutshell, run:

**Linux:**

```cd /usr/local/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

**FreeBSD:**

```pkg install -y wget && cd /usr/local/src && wget https://docs.phreaknet.org/script/phreaknet.sh && chmod +x phreaknet.sh && ./phreaknet.sh make```

> [!WARNING]
> BSD is not recommended for Asterisk, see below.

Then, you can use PhreakScript. Run ```phreaknet help``` or ```phreaknet examples``` to get started.

For a basic install, you can run `phreaknet install`. This installs all the pre-requisites for Asterisk and then downloads and builds Asterisk, completely non-interactively.

To install with DAHDI, run `phreaknet install --dahdi` (add `--drivers` to also restore drivers that were removed upstream in DAHDI 3).

For a guided, interactive installation, you can also run `phreaknet wizard`. The wizard will determine what installation options are best for you, based on your preferences. However, the wizard tool is not comprehensive for all available options.

For further details, please refer to the Docs: https://docs.phreaknet.org/#phreakscript

> [!TIP]
> There are a plethora of flags available that modify the installation process in various ways. It is highly recommended that you run ```phreaknet help``` and spend some time reading through the available options to craft the installation command that is appropriate for your system. Flags are available for most commonly used installation scenarios.

> [!IMPORTANT]
> PhreakScript must be run as root, even if Asterisk will not be installed to run as root.

> [!CAUTION]
> PhreakScript is primarily supported on Debian-based Linux systems, and DAHDI and Asterisk are best supported on these platforms.
> Limited support is available for other Linux distros (Fedora, RHEL, Rocky Linux, SUSE, Arch Linux, Alpine Linux, etc.).
> Extremely limited support exists for FreeBSD, and BSDs (and UNIX in general) are not recommended for running Asterisk/DAHDI - use Linux instead if possible.

### License

PhreakScript itself is licensed under the Apache 2.0 License. This includes any scripts in this repository.

However, any Asterisk modules ("C" code) in this repository (such as those that may be installed by PhreakScript) are licensed under the GNU General Public License Version 2 (GPLv2), per the Asterisk licensing terms.

> [!TIP]
> If you have copyright or licensing doubts, please refer to any copyrights and licensing terms in individual source files.

### Documentation

Please refer to the PhreakNet Asterisk documentation for PhreakScript-specific module documentation: https://asterisk.phreaknet.org/

### Change Log

Please run ```phreaknet about```

> [!NOTE]
> Note that the change log is no longer updated for every change. For a complete history, please refer to the commit history.

### Reporting Issues

To report an issue, you may do one of the following:

- (Preferred) Cut us a ticket at InterLinked Issues: https://issues.interlinked.us/

  Choose "PhreakScript" as the category, *not* Asterisk, DAHDI, or anything else!

- Open a new issue in this GitHub repository

### Pull Requests

Please see "Contributing to PhreakScript" in the Docs: https://docs.phreaknet.org/#contributions
