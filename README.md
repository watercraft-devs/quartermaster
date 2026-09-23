# Quartermaster

A daemon for activating and deactivating device specific daemons based on device connection state, for Linux systems that don't use systemd.




## Motivation:

Packages that ship with udev rules specific to systemd result in inert directives, an init script fixes the startup but doesn't fix connection state based activation and deactivation.


## Behavior:

Quartermaster uses new udev rules to apply user configured state changes to a device specific daemon when it detects that the expected type of device is connected.

Quartermaster is opt in, doesn't touch processes that it did not start, and handles everything via existing udev events.


The `udev` rules in `/etc/udev/rules.d/` describe specific actions that will be taken when triggered by relevant device connections.

The configs in `/etc/quartermaster.d/`  tell quartermaster which binary to look at, how to count relevant devices, etc...



Example rules that are enabled and disabled through cli get shipped to: 
`/usr/local/share/quartermaster/rules`

## Config and Rules Example
`/etc/quartermaster.d/ipp-usb.conf`

Says what to run, what device to look for a trigger, and when to stop.

**(this example and the shipped one is specific to one kind of Canon printer `:070104:` so that ID has to be changed to work for others, it will be generalized in 0.2.0)**

`match` is passed directly to `udevadm trigger --dry-run --verbose` so anything udev can do, the config can express.
```
exec /usr/sbin/ipp-usb 
match --subsystem-match=usb --property-match=ID_USB_INTERFACES=:070104:
stop_when none_present
```

---

`/etc/udev/rules.d/71-quartermaster-ipp-usb.rules`

This says exactly when add and remove events will happen. 

It needs 2 rules because `ID_USB_INTERFACES` is set during device setup and is gone by the time the device is removed. 

The add rule matches IPP-over-USB `ipp-usb` interface class. 

The remove rule matches the vendor and product so they survive teardown.

```
ACTION=="add", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device",
ENV{ID_USB_INTERFACES}==":070104:",
OWNER="root", GROUP="lp", MODE="0664",
RUN+="/usr/local/sbin/quartermaster-notify ipp-usb"

ACTION=="remove", SUBSYSTEM=="usb", ENV{DEVTYPE}=="usb_device",
ENV{PRODUCT}=="4a9/188d/*",
RUN+="/usr/local/sbin/quartermaster-notify ipp-usb"
```




## Requirements
- `udev`
- `sysvinit` style init system
- Linux specific `/proc/.../comm`

## Install

```sh
# cd to your quartermaster github clone first then make
make
sudo make install 
```


## Usage:

```
quartermaster                         run the daemon
quartermaster enable <target_name>    install udev rule for target
quartermaster disable <target_name>   remove udev rule for target
quartermaster list                    show available and active rules for targets
```

NOTE: Since this touches `udev` rules it needs `sudo` permissions to function.


## Uninstall
```sh
# cd to your quartermaster github clone first then make
sudo make uninstall
```




## 0.1.0 Known shortcomings

- no init script, planned for 0.2.0

- no backoff, targets that crash on start are retried each event at the bounded rate, ungraceful, will be fixed soon

- `quartermaster disable <target_name>` doesn't stop `target_name` in the case that its running, it only stops the udev events from occurring by removing the rules

- only one match field for now, can't express alternative add or remove conditions, possible in the future

- state file assumes `comm` never has any spaces in it, ok for anything right now but should probably be handled in the future

## License

**GPL 3.0**





