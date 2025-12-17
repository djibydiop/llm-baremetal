# Hardware Test Guide - WiFi 6 Bare-Metal

**Target**: Real hardware with Intel AX200/AX201 WiFi 6 card  
**Date**: December 16, 2025  
**Status**: Ready for testing

## Prerequisites

### 1. Hardware Requirements
- **PC with Intel AX200/AX201/AX210** WiFi card
  - Common in laptops from 2020+
  - Check: Device Manager → Network Adapters → "Intel Wi-Fi 6 AX200"
- **UEFI firmware** (not Legacy BIOS)
- **USB drive** (128 MB minimum, will be erased)
- **WiFi network** (WPA2-PSK or Open) for testing

### 2. Software Requirements
- **Rufus** (Windows USB creation tool)
- **Intel WiFi Firmware**: `iwlwifi-cc-a0-72.ucode`
- **llm-baremetal-usb.img** (created by make usb-image)

## Step 1: Download WiFi Firmware

The Intel AX200 requires firmware to function. Download it:

### Option A: Direct Download
```powershell
# Download from kernel.org
$url = "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/iwlwifi-cc-a0-72.ucode"
Invoke-WebRequest -Uri $url -OutFile "iwlwifi-cc-a0-72.ucode"
```

### Option B: From Linux Firmware Repository
```bash
git clone https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git
cd linux-firmware
cp iwlwifi-cc-a0-72.ucode /path/to/llm-baremetal/
```

**File Details**:
- **Name**: iwlwifi-cc-a0-72.ucode
- **Size**: ~450 KB
- **Version**: API 72 (latest for AX200)
- **Format**: Intel TLV format

## Step 2: Create USB Bootable Image

### Option A: Windows (Recommended)

```powershell
# In llm-baremetal directory
cd C:\Users\djibi\Desktop\baremetal\llm-baremetal

# Create USB image with WiFi firmware
wsl bash -c 'make usb-image'

# Mount image and add firmware
$driveLetter = (Mount-DiskImage -ImagePath "$PWD\llm-baremetal-usb.img" -PassThru | Get-Volume).DriveLetter
Copy-Item "iwlwifi-cc-a0-72.ucode" "${driveLetter}:\"
Dismount-DiskImage -ImagePath "$PWD\llm-baremetal-usb.img"

Write-Host "Image ready: llm-baremetal-usb.img" -ForegroundColor Green
```

### Option B: Linux

```bash
# Create image
make usb-image

# Mount and add firmware
sudo mkdir -p /mnt/llm
sudo mount -o loop llm-baremetal-usb.img /mnt/llm
sudo cp iwlwifi-cc-a0-72.ucode /mnt/llm/
sudo umount /mnt/llm

echo "Image ready"
```

## Step 3: Flash USB Drive with Rufus

1. **Open Rufus** (download from https://rufus.ie if needed)

2. **Select USB Drive**
   - Device: [Your USB drive]
   - ⚠️ **All data will be erased**

3. **Boot Selection**
   - Click "SELECT"
   - Choose: `llm-baremetal-usb.img`
   - Boot selection: "Disk or ISO image (DD Image)"

4. **Settings**
   - Partition scheme: **GPT**
   - Target system: **UEFI (non CSM)**
   - File system: **FAT32**

5. **Flash**
   - Click **START**
   - Wait 1-2 minutes
   - Click **CLOSE** when done

## Step 4: Boot from USB

### A. Configure BIOS/UEFI

1. **Restart PC** and enter BIOS/UEFI:
   - Press `F2`, `Del`, or `F12` during boot
   - (Key varies by manufacturer)

2. **Disable Secure Boot**:
   - Navigate to Security tab
   - Secure Boot → Disabled
   - Save and exit

3. **Boot Order**:
   - Move USB drive to first position
   - Or press F12 for boot menu

### B. Boot and Monitor

1. **Reboot** with USB inserted

2. **Observe Boot Screen**:
   ```
   llama-baremetal v1.0
   Initializing UEFI environment...
   
   [WIFI] Detecting Intel WiFi device...
   [WIFI] ✓ Found Intel WiFi at 00:14.3
   [WIFI] Vendor: 0x8086, Device: 0x2723
   [WIFI] MAC: XX:XX:XX:XX:XX:XX
   [WIFI] Status: ✓ DETECTED
   
   [FIRMWARE] Loading iwlwifi-cc-a0-72.ucode...
   [FIRMWARE] ✓ Loaded 450 KB
   [FIRMWARE] Parsing TLV sections...
   [FIRMWARE] ✓ CODE section: 320 KB
   [FIRMWARE] ✓ DATA section: 100 KB
   [FIRMWARE] Uploading to device via DMA...
   [FIRMWARE] ✓ Upload complete
   [FIRMWARE] Starting firmware...
   [FIRMWARE] ✓ Firmware ready
   
   [WIFI] Radio ON
   [WIFI] Scanning for networks...
   ```

3. **Expected Output**:
   - WiFi detection success
   - Firmware loading success
   - Network scan results (if any APs nearby)

## Step 5: Test Connection (If Implemented)

If connection code is active:

```
[WIFI] ═══ CONNECTING TO NETWORK ═══
[WIFI] SSID: YourNetwork
[WIFI] Step 1/4: 802.11 Authentication...
[WIFI] ✓ Authentication successful
[WIFI] Step 2/4: 802.11 Association...
[WIFI] ✓ Association successful
[WIFI] Step 3/4: WPA2 handshake...
[WIFI] PMK derived
[WIFI] SNonce generated
[WIFI] PTK derived
[WIFI] ✓ WPA2 keys installed
[WIFI] Step 4/4: Connection complete
[WIFI] ═══════════════════════════════
[WIFI] ✓ CONNECTED TO YourNetwork
[WIFI] BSSID: XX:XX:XX:XX:XX:XX
[WIFI] Security: WPA2-PSK
[WIFI] Signal: -50 dBm
[WIFI] ═══════════════════════════════
```

## Troubleshooting

### Issue 1: WiFi Not Detected
**Symptom**: `[WIFI] ✗ No Intel WiFi device found`

**Solutions**:
1. Check WiFi card is Intel AX200/AX201/AX210
2. Ensure WiFi is enabled in BIOS
3. Try different PCI slot (if desktop)
4. Check Device Manager in Windows to verify PCI ID

### Issue 2: Firmware Load Failed
**Symptom**: `[FIRMWARE] ✗ Failed to load firmware`

**Solutions**:
1. Verify firmware file is on USB root directory
2. Check filename: `iwlwifi-cc-a0-72.ucode` (exact match)
3. Verify file size (~450 KB)
4. Re-download firmware if corrupted

### Issue 3: No Networks Found
**Symptom**: Scan completes but finds 0 networks

**Solutions**:
1. Ensure WiFi router is powered on
2. Check 2.4 GHz band is enabled (scan only checks 2.4 GHz)
3. Move closer to router
4. Try different channel on router (1, 6, or 11)

### Issue 4: Association Timeout
**Symptom**: `[WIFI] ⚠ Association timeout`

**Solutions**:
1. This is expected - EAPOL frame exchange not fully implemented
2. Check hardware registers are correct for your specific card
3. May need firmware-specific command format adjustments

### Issue 5: System Hangs
**Symptom**: Screen freezes after WiFi init

**Solutions**:
1. Add serial output: Boot with serial cable connected
2. Disable WiFi init temporarily to isolate issue
3. Check DMA buffer allocation (may run out of memory)

## Data Collection

### Serial Output Capture

For detailed debugging, use serial output:

1. **Hardware**: USB-to-Serial adapter (CH340/FTDI)
2. **Connection**: 
   - TX → RX
   - RX → TX
   - GND → GND
3. **Software**: PuTTY, screen, or minicom
4. **Settings**: 115200 baud, 8N1

**Capture to File**:
```bash
screen -L /dev/ttyUSB0 115200
# Output saved to screenlog.0
```

### Success Criteria

✅ **Minimum Success**:
- WiFi device detected
- Firmware loaded successfully
- Radio turned ON
- MAC address read

✅ **Good Success**:
- All of above +
- Network scan successful
- At least 1 network found
- SSID/BSSID displayed correctly

✅ **Full Success**:
- All of above +
- Authentication successful
- Association successful
- WPA2 handshake completed (even if stubbed)
- State = CONNECTED

## Next Steps After Hardware Test

### If WiFi Detection Works
→ Proceed to implement DHCP client
→ Test data transmission

### If Firmware Loading Works
→ Debug scan/connection issues
→ Analyze hardware command responses

### If Basic Detection Fails
→ Check PCI enumeration code
→ Verify BAR0 MMIO mapping
→ Test with different WiFi card model

## Expected Timeline

- **Setup**: 15 minutes (USB creation, BIOS config)
- **First Boot Test**: 5 minutes
- **Debugging**: 30-60 minutes (if issues occur)
- **Documentation**: 15 minutes

**Total**: ~1-2 hours for complete hardware validation

## Safety Notes

⚠️ **This is bare-metal code running without OS protection**:
- No data will be permanently modified on your system
- WiFi card will be reset on power cycle
- USB boot is non-destructive to main OS
- Can always remove USB and boot normally

✅ **Safe to test on**:
- Development laptops
- Test machines
- Personal computers (no data loss risk)

❌ **Not recommended**:
- Production servers
- Critical systems
- Machines with important unsaved work

---

**Ready to test!** Flash the USB and boot. Report results for further optimization.

Made in Dakar, Senegal 🇸🇳
