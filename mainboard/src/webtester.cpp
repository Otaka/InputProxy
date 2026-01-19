#include <thread>
#include <iostream>
#include <sstream>
#include "webtester.h"
#include "httplib.h"
#include "kbdkeys.h"

// HTML generator for the web interface
std::string generateHtml() {
    std::ostringstream html;
    
    html << R"html(<!DOCTYPE html>
<html>
<head>
    <title>HID Device Tester</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            max-width: 1000px;
            margin: 50px auto;
            padding: 20px;
            background-color: #f5f5f5;
        }
        .container {
            background: white;
            padding: 30px;
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }
        h1 {
            color: #333;
            border-bottom: 3px solid #4CAF50;
            padding-bottom: 10px;
        }
        .controls {
            margin: 20px 0;
            padding: 15px;
            background: #f9f9f9;
            border-radius: 5px;
        }
        label {
            display: inline-block;
            width: 120px;
            font-weight: bold;
            color: #555;
        }
        select {
            padding: 8px 12px;
            border: 2px solid #ddd;
            border-radius: 4px;
            font-size: 14px;
            margin: 10px 5px;
            min-width: 150px;
        }
        select:disabled {
            background: #e0e0e0;
            cursor: not-allowed;
        }
        .keyboard-section {
            margin-top: 30px;
        }
        .key-category {
            margin: 20px 0;
            padding: 15px;
            background: #fafafa;
            border-radius: 8px;
            border: 1px solid #e0e0e0;
        }
        .key-category h3 {
            margin-top: 0;
            color: #555;
            font-size: 16px;
            border-bottom: 2px solid #ddd;
            padding-bottom: 5px;
        }
        .key-row {
            margin: 5px 0;
            display: flex;
            gap: 5px;
            flex-wrap: wrap;
        }
        .key {
            padding: 10px 15px;
            border: 2px solid #ddd;
            border-radius: 5px;
            background: linear-gradient(to bottom, #fff, #f0f0f0);
            cursor: pointer;
            user-select: none;
            font-weight: bold;
            min-width: 45px;
            text-align: center;
            transition: all 0.1s;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            font-size: 11px;
        }
        .key:hover {
            background: linear-gradient(to bottom, #f0f0f0, #e0e0e0);
            border-color: #4CAF50;
        }
        .key:active, .key.pressed {
            background: #4CAF50;
            color: white;
            border-color: #45a049;
            box-shadow: inset 0 2px 4px rgba(0,0,0,0.2);
            transform: translateY(2px);
        }
        .key.wide {
            min-width: 100px;
        }
        .key.modifier {
            background: linear-gradient(to bottom, #e3f2fd, #bbdefb);
        }
        .key.modifier:active, .key.modifier.pressed {
            background: #2196F3;
            border-color: #1976D2;
        }
        .key.consumer {
            background: linear-gradient(to bottom, #fff3e0, #ffe0b2);
        }
        .key.consumer:active, .key.consumer.pressed {
            background: #ff9800;
            border-color: #f57c00;
        }
        .status {
            margin-top: 20px;
            padding: 10px;
            background: #e8f5e9;
            border-left: 4px solid #4CAF50;
            border-radius: 4px;
        }
        .gamepad-section {
            margin-top: 30px;
        }
        .gamepad-controls {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
            gap: 20px;
            margin: 20px 0;
        }
        .control-group {
            background: #fafafa;
            padding: 15px;
            border-radius: 8px;
            border: 1px solid #e0e0e0;
        }
        .control-group h3 {
            margin-top: 0;
            color: #555;
            font-size: 16px;
            border-bottom: 2px solid #ddd;
            padding-bottom: 5px;
        }
        .slider-container {
            margin: 15px 0;
        }
        .slider-label {
            display: flex;
            justify-content: space-between;
            margin-bottom: 5px;
            font-weight: bold;
            color: #666;
        }
        .slider-value {
            color: #4CAF50;
            font-family: monospace;
        }
        .slider {
            width: 100%;
            height: 8px;
            border-radius: 4px;
            background: #ddd;
            outline: none;
            -webkit-appearance: none;
        }
        .slider::-webkit-slider-thumb {
            -webkit-appearance: none;
            appearance: none;
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4CAF50;
            cursor: pointer;
            box-shadow: 0 2px 4px rgba(0,0,0,0.2);
        }
        .slider::-moz-range-thumb {
            width: 20px;
            height: 20px;
            border-radius: 50%;
            background: #4CAF50;
            cursor: pointer;
            box-shadow: 0 2px 4px rgba(0,0,0,0.2);
            border: none;
        }
        .button-grid {
            display: grid;
            grid-template-columns: repeat(3, 1fr);
            gap: 10px;
            margin: 15px 0;
        }
        .gamepad-button {
            padding: 15px;
            border: 2px solid #ddd;
            border-radius: 8px;
            background: linear-gradient(to bottom, #fff, #f0f0f0);
            cursor: pointer;
            text-align: center;
            font-weight: bold;
            transition: all 0.1s;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
            user-select: none;
        }
        .gamepad-button:hover {
            background: linear-gradient(to bottom, #f0f0f0, #e0e0e0);
            border-color: #4CAF50;
        }
        .gamepad-button:active, .gamepad-button.active {
            background: #4CAF50;
            color: white;
            border-color: #45a049;
            box-shadow: inset 0 2px 4px rgba(0,0,0,0.2);
            transform: translateY(2px);
        }
        .reboot-button {
            padding: 12px 24px;
            border: 2px solid #f44336;
            border-radius: 5px;
            background: linear-gradient(to bottom, #ff5252, #f44336);
            color: white;
            cursor: pointer;
            font-weight: bold;
            font-size: 14px;
            transition: all 0.2s;
            box-shadow: 0 2px 4px rgba(0,0,0,0.2);
            display: inline-block;
            user-select: none;
            margin-top: 10px;
        }
        .reboot-button:hover {
            background: linear-gradient(to bottom, #ff1744, #d32f2f);
            border-color: #d32f2f;
            box-shadow: 0 4px 8px rgba(0,0,0,0.3);
        }
        .reboot-button:active {
            background: #c62828;
            border-color: #b71c1c;
            box-shadow: inset 0 2px 4px rgba(0,0,0,0.3);
            transform: translateY(2px);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>HID Device Tester</h1>
        
        <div class="controls">
            <div>
                <label>Device:</label>
                <select id="deviceSelect">
                    <option value="0">Device 0</option>    
                    <option value="1">Device 1</option>
                    <option value="2">Device 2</option>
                    <option value="3">Device 3</option>
                    <option value="4">Device 4</option>
                    <option value="5">Device 5</option>
                    <option value="6">Device 6</option>
                    <option value="7">Device 7</option>
                    <option value="8">Device 8</option>
                    <option value="9">Device 9</option>
                    <option value="10">Device 10</option>
                </select>
            </div>
            <div>
                <label>Device Type:</label>
                <select id="typeSelect">
                    <option value="keyboard">Keyboard</option>
                    <option value="mouse">Mouse</option>
                    <option value="gamepad">XInput Gamepad</option>
                    <option value="hidgamepad">HID Gamepad</option>
                </select>
            </div>
            <div>
                <button class="reboot-button" onclick="rebootToFlashMode()">Reboot to Flash Mode</button>
            </div>
        </div>

        <div class="keyboard-section" id="keyboardSection">
            <h2>Keyboard Tester - All Keys</h2>
            <p>Click and hold keys to test. All keys from HID specification included.</p>
            
            <!-- Main Keyboard Layout -->
            <div class="key-category">
                <h3>Main Keyboard</h3>
                <div class="key-row">
                    <div class="key" data-key="0x29">ESC</div>
                    <div class="key" data-key="0x3A">F1</div>
                    <div class="key" data-key="0x3B">F2</div>
                    <div class="key" data-key="0x3C">F3</div>
                    <div class="key" data-key="0x3D">F4</div>
                    <div class="key" data-key="0x3E">F5</div>
                    <div class="key" data-key="0x3F">F6</div>
                    <div class="key" data-key="0x40">F7</div>
                    <div class="key" data-key="0x41">F8</div>
                    <div class="key" data-key="0x42">F9</div>
                    <div class="key" data-key="0x43">F10</div>
                    <div class="key" data-key="0x44">F11</div>
                    <div class="key" data-key="0x45">F12</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x35">Quote</div>
                    <div class="key" data-key="0x1E">1</div>
                    <div class="key" data-key="0x1F">2</div>
                    <div class="key" data-key="0x20">3</div>
                    <div class="key" data-key="0x21">4</div>
                    <div class="key" data-key="0x22">5</div>
                    <div class="key" data-key="0x23">6</div>
                    <div class="key" data-key="0x24">7</div>
                    <div class="key" data-key="0x25">8</div>
                    <div class="key" data-key="0x26">9</div>
                    <div class="key" data-key="0x27">0</div>
                    <div class="key" data-key="0x2D">-</div>
                    <div class="key" data-key="0x2E">=</div>
                    <div class="key wide" data-key="0x2A">BKSP</div>
                </div>
                <div class="key-row">
                    <div class="key wide" data-key="0x2B">TAB</div>
                    <div class="key" data-key="0x14">Q</div>
                    <div class="key" data-key="0x1A">W</div>
                    <div class="key" data-key="0x08">E</div>
                    <div class="key" data-key="0x15">R</div>
                    <div class="key" data-key="0x17">T</div>
                    <div class="key" data-key="0x1C">Y</div>
                    <div class="key" data-key="0x18">U</div>
                    <div class="key" data-key="0x0C">I</div>
                    <div class="key" data-key="0x12">O</div>
                    <div class="key" data-key="0x13">P</div>
                    <div class="key" data-key="0x2F">[</div>
                    <div class="key" data-key="0x30">]</div>
                    <div class="key" data-key="0x31">\</div>
                </div>
                <div class="key-row">
                    <div class="key modifier wide" data-key="0x39">CAPS</div>
                    <div class="key" data-key="0x04">A</div>
                    <div class="key" data-key="0x16">S</div>
                    <div class="key" data-key="0x07">D</div>
                    <div class="key" data-key="0x09">F</div>
                    <div class="key" data-key="0x0A">G</div>
                    <div class="key" data-key="0x0B">H</div>
                    <div class="key" data-key="0x0D">J</div>
                    <div class="key" data-key="0x0E">K</div>
                    <div class="key" data-key="0x0F">L</div>
                    <div class="key" data-key="0x33">;</div>
                    <div class="key" data-key="0x34">'</div>
                    <div class="key wide" data-key="0x28">ENTER</div>
                </div>
                <div class="key-row">
                    <div class="key modifier wide" data-key="0xE1">SHIFT</div>
                    <div class="key" data-key="0x1D">Z</div>
                    <div class="key" data-key="0x1B">X</div>
                    <div class="key" data-key="0x06">C</div>
                    <div class="key" data-key="0x19">V</div>
                    <div class="key" data-key="0x05">B</div>
                    <div class="key" data-key="0x11">N</div>
                    <div class="key" data-key="0x10">M</div>
                    <div class="key" data-key="0x36">,</div>
                    <div class="key" data-key="0x37">.</div>
                    <div class="key" data-key="0x38">/</div>
                    <div class="key modifier wide" data-key="0xE5">SHIFT</div>
                </div>
                <div class="key-row">
                    <div class="key modifier" data-key="0xE0">CTRL</div>
                    <div class="key modifier" data-key="0xE3">WIN</div>
                    <div class="key modifier" data-key="0xE2">ALT</div>
                    <div class="key wide" data-key="0x2C" style="min-width: 250px;">SPACE</div>
                    <div class="key modifier" data-key="0xE6">ALT</div>
                    <div class="key modifier" data-key="0xE7">WIN</div>
                    <div class="key modifier" data-key="0xE4">CTRL</div>
                </div>
            </div>

            <!-- Navigation Block -->
            <div class="key-category">
                <h3>Navigation & Editing</h3>
                <div class="key-row">
                    <div class="key" data-key="0x46">PrtSc</div>
                    <div class="key" data-key="0x47">ScrLk</div>
                    <div class="key" data-key="0x48">Pause</div>
                    <div class="key" data-key="0x49">Ins</div>
                    <div class="key" data-key="0x4A">Home</div>
                    <div class="key" data-key="0x4B">PgUp</div>
                    <div class="key" data-key="0x4C">Del</div>
                    <div class="key" data-key="0x4D">End</div>
                    <div class="key" data-key="0x4E">PgDn</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x52">↑</div>
                    <div class="key" data-key="0x51">↓</div>
                    <div class="key" data-key="0x50">←</div>
                    <div class="key" data-key="0x4F">→</div>
                </div>
            </div>

            <!-- Numpad -->
            <div class="key-category">
                <h3>Numpad</h3>
                <div class="key-row">
                    <div class="key modifier" data-key="0x53">NumLk</div>
                    <div class="key" data-key="0x54">÷</div>
                    <div class="key" data-key="0x55">×</div>
                    <div class="key" data-key="0x56">-</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x5F">7</div>
                    <div class="key" data-key="0x60">8</div>
                    <div class="key" data-key="0x61">9</div>
                    <div class="key" data-key="0x57">+</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x5C">4</div>
                    <div class="key" data-key="0x5D">5</div>
                    <div class="key" data-key="0x5E">6</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x59">1</div>
                    <div class="key" data-key="0x5A">2</div>
                    <div class="key" data-key="0x5B">3</div>
                    <div class="key" data-key="0x58">Ent</div>
                </div>
                <div class="key-row">
                    <div class="key wide" data-key="0x62">0</div>
                    <div class="key" data-key="0x63">.</div>
                </div>
            </div>

            <!-- Extended Function Keys -->
            <div class="key-category">
                <h3>Extended Function Keys (F13-F24)</h3>
                <div class="key-row">
                    <div class="key" data-key="0x68">F13</div>
                    <div class="key" data-key="0x69">F14</div>
                    <div class="key" data-key="0x6A">F15</div>
                    <div class="key" data-key="0x6B">F16</div>
                    <div class="key" data-key="0x6C">F17</div>
                    <div class="key" data-key="0x6D">F18</div>
                    <div class="key" data-key="0x6E">F19</div>
                    <div class="key" data-key="0x6F">F20</div>
                    <div class="key" data-key="0x70">F21</div>
                    <div class="key" data-key="0x71">F22</div>
                    <div class="key" data-key="0x72">F23</div>
                    <div class="key" data-key="0x73">F24</div>
                </div>
            </div>

            <!-- System & Special Keys -->
            <div class="key-category">
                <h3>System & Special Keys</h3>
                <div class="key-row">
                    <div class="key" data-key="0x74">Exec</div>
                    <div class="key" data-key="0x75">Help</div>
                    <div class="key" data-key="0x76">Menu</div>
                    <div class="key" data-key="0x77">Select</div>
                    <div class="key" data-key="0x78">Stop</div>
                    <div class="key" data-key="0x79">Again</div>
                    <div class="key" data-key="0x7A">Undo</div>
                    <div class="key" data-key="0x7B">Cut</div>
                    <div class="key" data-key="0x7C">Copy</div>
                    <div class="key" data-key="0x7D">Paste</div>
                    <div class="key" data-key="0x7E">Find</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x7F">Mute</div>
                    <div class="key" data-key="0x80">Vol+</div>
                    <div class="key" data-key="0x81">Vol-</div>
                    <div class="key" data-key="0x32">EUR1</div>
                    <div class="key" data-key="0x64">EUR2</div>
                    <div class="key" data-key="0x65">App</div>
                    <div class="key" data-key="0x66">Power</div>
                    <div class="key" data-key="0x67">KP=</div>
                </div>
            </div>

            <!-- International Keys -->
            <div class="key-category">
                <h3>International & Language Keys</h3>
                <div class="key-row">
                    <div class="key" data-key="0x87">Kanji1</div>
                    <div class="key" data-key="0x88">Kanji2</div>
                    <div class="key" data-key="0x89">Kanji3</div>
                    <div class="key" data-key="0x8A">Kanji4</div>
                    <div class="key" data-key="0x8B">Kanji5</div>
                    <div class="key" data-key="0x8C">Kanji6</div>
                    <div class="key" data-key="0x8D">Kanji7</div>
                    <div class="key" data-key="0x8E">Kanji8</div>
                    <div class="key" data-key="0x8F">Kanji9</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0x90">Lang1</div>
                    <div class="key" data-key="0x91">Lang2</div>
                    <div class="key" data-key="0x92">Lang3</div>
                    <div class="key" data-key="0x93">Lang4</div>
                    <div class="key" data-key="0x94">Lang5</div>
                    <div class="key" data-key="0x95">Lang6</div>
                    <div class="key" data-key="0x96">Lang7</div>
                    <div class="key" data-key="0x97">Lang8</div>
                    <div class="key" data-key="0x98">Lang9</div>
                </div>
            </div>

            <!-- Locking Keys -->
            <div class="key-category">
                <h3>Locking Keys</h3>
                <div class="key-row">
                    <div class="key modifier" data-key="0x82">LkCaps</div>
                    <div class="key modifier" data-key="0x83">LkNum</div>
                    <div class="key modifier" data-key="0x84">LkScr</div>
                </div>
            </div>

            <!-- Advanced Keypad -->
            <div class="key-category">
                <h3>Advanced Keypad Operations</h3>
                <div class="key-row">
                    <div class="key" data-key="0x85">KP,</div>
                    <div class="key" data-key="0x86">KP=</div>
                    <div class="key" data-key="0xB0">KP00</div>
                    <div class="key" data-key="0xB1">KP000</div>
                    <div class="key" data-key="0xB2">KP1000Sep</div>
                    <div class="key" data-key="0xB3">KPDecSep</div>
                    <div class="key" data-key="0xB4">KPCur</div>
                    <div class="key" data-key="0xB5">KPSub</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0xB6">KP(</div>
                    <div class="key" data-key="0xB7">KP)</div>
                    <div class="key" data-key="0xB8">KP{</div>
                    <div class="key" data-key="0xB9">KP}</div>
                    <div class="key" data-key="0xBA">KPTab</div>
                    <div class="key" data-key="0xBB">KPBksp</div>
                    <div class="key" data-key="0xBC">KPA</div>
                    <div class="key" data-key="0xBD">KPB</div>
                    <div class="key" data-key="0xBE">KPC</div>
                    <div class="key" data-key="0xBF">KPD</div>
                    <div class="key" data-key="0xC0">KPE</div>
                    <div class="key" data-key="0xC1">KPF</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0xC2">KP^</div>
                    <div class="key" data-key="0xC3">KP^^</div>
                    <div class="key" data-key="0xC4">KP%</div>
                    <div class="key" data-key="0xC5">KP<</div>
                    <div class="key" data-key="0xC6">KP></div>
                    <div class="key" data-key="0xC7">KP&</div>
                    <div class="key" data-key="0xC8">KP&&</div>
                    <div class="key" data-key="0xC9">KP|</div>
                    <div class="key" data-key="0xCA">KP||</div>
                    <div class="key" data-key="0xCB">KP:</div>
                    <div class="key" data-key="0xCC">KP#</div>
                    <div class="key" data-key="0xCD">KPSpc</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0xCE">KP@</div>
                    <div class="key" data-key="0xCF">KP!</div>
                    <div class="key" data-key="0xD0">KPMSto</div>
                    <div class="key" data-key="0xD1">KPMRcl</div>
                    <div class="key" data-key="0xD2">KPMClr</div>
                    <div class="key" data-key="0xD3">KPM+</div>
                    <div class="key" data-key="0xD4">KPM-</div>
                    <div class="key" data-key="0xD5">KPM*</div>
                    <div class="key" data-key="0xD6">KPM/</div>
                    <div class="key" data-key="0xD7">KP±</div>
                </div>
                <div class="key-row">
                    <div class="key" data-key="0xD8">KPClr</div>
                    <div class="key" data-key="0xD9">KPClrEnt</div>
                    <div class="key" data-key="0xDA">KPBin</div>
                    <div class="key" data-key="0xDB">KPOct</div>
                    <div class="key" data-key="0xDC">KPDec</div>
                    <div class="key" data-key="0xDD">KPHex</div>
                </div>
            </div>

            <!-- Consumer Control (Media Keys) -->
            <div class="key-category">
                <h3>Consumer Control (Media & System)</h3>
                <div class="key-row">
                    <div class="key consumer" data-key="611">Bright+</div>
                    <div class="key consumer" data-key="612">Bright-</div>
                    <div class="key consumer" data-key="676">Play</div>
                    <div class="key consumer" data-key="677">Pause</div>
                    <div class="key consumer" data-key="679">FFwd</div>
                    <div class="key consumer" data-key="680">Rewind</div>
                    <div class="key consumer" data-key="681">Next</div>
                    <div class="key consumer" data-key="682">Prev</div>
                    <div class="key consumer" data-key="683">Stop</div>
                    <div class="key consumer" data-key="684">Eject</div>
                    <div class="key consumer" data-key="705">Play/Pause</div>
                </div>
                <div class="key-row">
                    <div class="key consumer" data-key="726">Mute</div>
                    <div class="key consumer" data-key="733">Vol+</div>
                    <div class="key consumer" data-key="734">Vol-</div>
                    <div class="key consumer" data-key="894">Email</div>
                    <div class="key consumer" data-key="902">Calc</div>
                </div>
                <div class="key-row">
                    <div class="key consumer" data-key="1045">Search</div>
                    <div class="key consumer" data-key="1047">Home</div>
                    <div class="key consumer" data-key="1048">Back</div>
                    <div class="key consumer" data-key="1049">Forward</div>
                    <div class="key consumer" data-key="1050">ACStop</div>
                    <div class="key consumer" data-key="1051">Refresh</div>
                    <div class="key consumer" data-key="1054">Bookmark</div>
                </div>
            </div>

            <!-- Additional System Keys -->
            <div class="key-category">
                <h3>Additional System Keys</h3>
                <div class="key-row">
                    <div class="key" data-key="0x99">AltErase</div>
                    <div class="key" data-key="0x9A">SysReq</div>
                    <div class="key" data-key="0x9B">Cancel</div>
                    <div class="key" data-key="0x9C">Clear</div>
                    <div class="key" data-key="0x9D">Prior</div>
                    <div class="key" data-key="0x9E">Return</div>
                    <div class="key" data-key="0x9F">Sep</div>
                    <div class="key" data-key="0xA0">Out</div>
                    <div class="key" data-key="0xA1">Oper</div>
                    <div class="key" data-key="0xA2">ClrAgain</div>
                    <div class="key" data-key="0xA3">CrSel</div>
                    <div class="key" data-key="0xA4">ExSel</div>
                </div>
            </div>
        </div>

        <div class="mouse-section" id="mouseSection" style="display: none;">
            <h2>Mouse Tester</h2>
            <p>Test mouse buttons, movement, and scroll wheels.</p>
            
            <div class="gamepad-controls">
                <!-- Mouse Buttons -->
                <div class="control-group">
                    <h3>Mouse Buttons</h3>
                    <div class="button-grid">
                        <div class="gamepad-button" data-button="0">Left Button</div>
                        <div class="gamepad-button" data-button="1">Right Button</div>
                        <div class="gamepad-button" data-button="2">Middle Button</div>
                        <div class="gamepad-button" data-button="3">Back Button</div>
                        <div class="gamepad-button" data-button="4">Forward Button</div>
                    </div>
                </div>

                <!-- Mouse Movement -->
                <div class="control-group">
                    <h3>Mouse Movement</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Left</span>
                            <span class="slider-value" id="mouseXMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseXMinus" data-axis="5">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Right</span>
                            <span class="slider-value" id="mouseXPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseXPlus" data-axis="6">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Up</span>
                            <span class="slider-value" id="mouseYMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseYMinus" data-axis="7">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Down</span>
                            <span class="slider-value" id="mouseYPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseYPlus" data-axis="8">
                    </div>
                </div>

                <!-- Mouse Wheels -->
                <div class="control-group">
                    <h3>Scroll Wheels</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Wheel Down</span>
                            <span class="slider-value" id="mouseWheelMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseWheelMinus" data-axis="9">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Wheel Up</span>
                            <span class="slider-value" id="mouseWheelPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseWheelPlus" data-axis="10">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>H-Wheel Left</span>
                            <span class="slider-value" id="mouseHWheelMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseHWheelMinus" data-axis="11">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>H-Wheel Right</span>
                            <span class="slider-value" id="mouseHWheelPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="127" value="0" class="slider" id="mouseHWheelPlus" data-axis="12">
                    </div>
                </div>
            </div>
        </div>

        <div class="hidgamepad-section" id="hidgamepadSection" style="display: none;">
            <h2>HID Gamepad Tester</h2>
            <p>Test HID gamepad with HAT switch, buttons, and analog sticks.</p>
            
            <div class="gamepad-controls">
                <!-- HAT Switch (D-Pad) -->
                <div class="control-group">
                    <h3>HAT Switch (D-Pad)</h3>
                    <div class="button-grid">
                        <div class="gamepad-button" data-hidhat="up">HAT Up</div>
                        <div class="gamepad-button" data-hidhat="down">HAT Down</div>
                        <div class="gamepad-button" data-hidhat="left">HAT Left</div>
                        <div class="gamepad-button" data-hidhat="right">HAT Right</div>
                    </div>
                </div>

                <!-- Action Buttons -->
                <div class="control-group">
                    <h3>Action Buttons</h3>
                    <div class="button-grid">
                        <div class="gamepad-button" data-hidbutton="0">Button 1</div>
                        <div class="gamepad-button" data-hidbutton="1">Button 2</div>
                        <div class="gamepad-button" data-hidbutton="2">Button 3</div>
                        <div class="gamepad-button" data-hidbutton="3">Button 4</div>
                        <div class="gamepad-button" data-hidbutton="4">Button 5</div>
                        <div class="gamepad-button" data-hidbutton="5">Button 6</div>
                        <div class="gamepad-button" data-hidbutton="6">Button 7</div>
                        <div class="gamepad-button" data-hidbutton="7">Button 8</div>
                        <div class="gamepad-button" data-hidbutton="8">Button 9</div>
                        <div class="gamepad-button" data-hidbutton="9">Button 10</div>
                        <div class="gamepad-button" data-hidbutton="10">Button 11</div>
                        <div class="gamepad-button" data-hidbutton="11">Button 12</div>
                        <div class="gamepad-button" data-hidbutton="12">Button 13</div>
                        <div class="gamepad-button" data-hidbutton="13">Button 14</div>
                        <div class="gamepad-button" data-hidbutton="14">Button 15</div>
                        <div class="gamepad-button" data-hidbutton="15">Button 16</div>
                        <div class="gamepad-button" data-hidbutton="16">Button 17</div>
                        <div class="gamepad-button" data-hidbutton="17">Button 18</div>
                        <div class="gamepad-button" data-hidbutton="18">Button 19</div>
                        <div class="gamepad-button" data-hidbutton="19">Button 20</div>
                        <div class="gamepad-button" data-hidbutton="20">Button 21</div>
                        <div class="gamepad-button" data-hidbutton="21">Button 22</div>
                        <div class="gamepad-button" data-hidbutton="22">Button 23</div>
                        <div class="gamepad-button" data-hidbutton="23">Button 24</div>
                        <div class="gamepad-button" data-hidbutton="24">Button 25</div>
                        <div class="gamepad-button" data-hidbutton="25">Button 26</div>
                        <div class="gamepad-button" data-hidbutton="26">Button 27</div>
                        <div class="gamepad-button" data-hidbutton="27">Button 28</div>
                        <div class="gamepad-button" data-hidbutton="28">Button 29</div>
                        <div class="gamepad-button" data-hidbutton="29">Button 30</div>
                        <div class="gamepad-button" data-hidbutton="30">Button 31</div>
                        <div class="gamepad-button" data-hidbutton="31">Button 32</div>
                    </div>
                </div>

                <!-- Left Analog Stick -->
                <div class="control-group">
                    <h3>Left Analog Stick</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Axis</span>
                            <span class="slider-value" id="hidLeftStickX-value">128</span>
                        </div>
                        <input type="range" min="0" max="255" value="128" class="slider" id="hidLeftStickX" data-hidaxis="0">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Axis</span>
                            <span class="slider-value" id="hidLeftStickY-value">128</span>
                        </div>
                        <input type="range" min="0" max="255" value="128" class="slider" id="hidLeftStickY" data-hidaxis="1">
                    </div>
                </div>

                <!-- Right Analog Stick -->
                <div class="control-group">
                    <h3>Right Analog Stick</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Axis</span>
                            <span class="slider-value" id="hidRightStickX-value">128</span>
                        </div>
                        <input type="range" min="0" max="255" value="128" class="slider" id="hidRightStickX" data-hidaxis="2">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Axis</span>
                            <span class="slider-value" id="hidRightStickY-value">128</span>
                        </div>
                        <input type="range" min="0" max="255" value="128" class="slider" id="hidRightStickY" data-hidaxis="3">
                    </div>
                </div>

                <!-- Triggers -->
                <div class="control-group">
                    <h3>Triggers / Z-Axis</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Left Trigger (Z)</span>
                            <span class="slider-value" id="hidLeftTrigger-value">0</span>
                        </div>
                        <input type="range" min="0" max="255" value="0" class="slider" id="hidLeftTrigger" data-hidaxis="4">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Right Trigger (Rz)</span>
                            <span class="slider-value" id="hidRightTrigger-value">0</span>
                        </div>
                        <input type="range" min="0" max="255" value="0" class="slider" id="hidRightTrigger" data-hidaxis="5">
                    </div>
                </div>
            </div>
        </div>

        <div class="gamepad-section" id="gamepadSection" style="display: none;">
            <h2>XInput Gamepad Tester</h2>
            <p>Adjust sliders and press buttons to test gamepad inputs (4 gamepads supported).</p>
            
            <div class="gamepad-controls">
                <!-- Buttons -->
                <div class="control-group">
                    <h3>Buttons</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>A</span>
                            <span class="slider-value" id="btnA-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnA" data-axis="12">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>B</span>
                            <span class="slider-value" id="btnB-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnB" data-axis="13">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X</span>
                            <span class="slider-value" id="btnX-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnX" data-axis="14">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y</span>
                            <span class="slider-value" id="btnY-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnY" data-axis="15">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Left Bumper</span>
                            <span class="slider-value" id="btnLB-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnLB" data-axis="8">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Right Bumper</span>
                            <span class="slider-value" id="btnRB-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnRB" data-axis="9">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Back</span>
                            <span class="slider-value" id="btnBack-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnBack" data-axis="5">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Start</span>
                            <span class="slider-value" id="btnStart-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnStart" data-axis="4">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Left Stick Click</span>
                            <span class="slider-value" id="btnLS-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnLS" data-axis="6">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Right Stick Click</span>
                            <span class="slider-value" id="btnRS-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnRS" data-axis="7">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Guide</span>
                            <span class="slider-value" id="btnGuide-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnGuide" data-axis="10">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>D-Pad Up</span>
                            <span class="slider-value" id="btnDUp-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnDUp" data-axis="0">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>D-Pad Down</span>
                            <span class="slider-value" id="btnDDown-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnDDown" data-axis="1">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>D-Pad Left</span>
                            <span class="slider-value" id="btnDLeft-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnDLeft" data-axis="2">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>D-Pad Right</span>
                            <span class="slider-value" id="btnDRight-value">0</span>
                        </div>
                        <input type="range" min="0" max="1" step="1" value="0" class="slider" id="btnDRight" data-axis="3">
                    </div>
                </div>

                <!-- Triggers -->
                <div class="control-group">
                    <h3>Triggers</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Left Trigger</span>
                            <span class="slider-value" id="triggerL-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="triggerL" data-axis="16">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Right Trigger</span>
                            <span class="slider-value" id="triggerR-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="triggerR" data-axis="17">
                    </div>
                </div>

                <!-- Left Stick -->
                <div class="control-group">
                    <h3>Left Stick</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Left</span>
                            <span class="slider-value" id="joystickLXMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickLXMinus" data-axis="18">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Right</span>
                            <span class="slider-value" id="joystickLXPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickLXPlus" data-axis="19">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Down</span>
                            <span class="slider-value" id="joystickLYMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickLYMinus" data-axis="20">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Up</span>
                            <span class="slider-value" id="joystickLYPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickLYPlus" data-axis="21">
                    </div>
                </div>

                <!-- Right Stick -->
                <div class="control-group">
                    <h3>Right Stick</h3>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Left</span>
                            <span class="slider-value" id="joystickRXMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickRXMinus" data-axis="22">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>X Right</span>
                            <span class="slider-value" id="joystickRXPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickRXPlus" data-axis="23">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Down</span>
                            <span class="slider-value" id="joystickRYMinus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickRYMinus" data-axis="24">
                    </div>
                    <div class="slider-container">
                        <div class="slider-label">
                            <span>Y Up</span>
                            <span class="slider-value" id="joystickRYPlus-value">0</span>
                        </div>
                        <input type="range" min="0" max="1000" value="0" class="slider" id="joystickRYPlus" data-axis="25">
                    </div>
                </div>
            </div>
        </div>

        <div class="status" id="status">
            Ready. Select a device and press keys to test.
        </div>
    </div>

    <script>
        const deviceSelect = document.getElementById('deviceSelect');
        const typeSelect = document.getElementById('typeSelect');
        const status = document.getElementById('status');
        const keys = document.querySelectorAll('.key');
        const keyboardSection = document.getElementById('keyboardSection');
        const mouseSection = document.getElementById('mouseSection');
        const gamepadSection = document.getElementById('gamepadSection');
        
        let pressedKeys = new Set();

        const hidgamepadSection = document.getElementById('hidgamepadSection');

        // Reboot to flash mode function
        function rebootToFlashMode() {
            if (!confirm('Reboot Pico to Flash Mode?')) {
                return;
            }
            
            updateStatus('Sending reboot command...', true);
            
            fetch('/rebootflashmode')
                .then(response => {
                    if (response.ok) {
                        updateStatus('Reboot command sent successfully! Pico is rebooting to flash mode.', false);
                    } else {
                        updateStatus('Error: Failed to send reboot command (HTTP ' + response.status + ')', false);
                    }
                })
                .catch(err => {
                    console.error('Error sending reboot request:', err);
                    updateStatus('Error: ' + err.message, false);
                });
        }

        // Toggle between keyboard, mouse, and gamepad
        typeSelect.addEventListener('change', () => {
            const type = typeSelect.value;
            keyboardSection.style.display = 'none';
            mouseSection.style.display = 'none';
            gamepadSection.style.display = 'none';
            hidgamepadSection.style.display = 'none';
            
            if (type === 'keyboard') {
                keyboardSection.style.display = 'block';
                status.textContent = 'Switched to Keyboard mode';
            } else if (type === 'mouse') {
                mouseSection.style.display = 'block';
                status.textContent = 'Switched to Mouse mode';
            } else if (type === 'gamepad') {
                gamepadSection.style.display = 'block';
                status.textContent = 'Switched to XInput Gamepad mode';
            } else if (type === 'hidgamepad') {
                hidgamepadSection.style.display = 'block';
                status.textContent = 'Switched to HID Gamepad mode';
            }
        });

        function sendAxisRequest(device, axis, value) {
            const url = `/setaxis?device=${device}&axis=${axis}&value=${value}`;
            
            fetch(url)
                .catch(err => {
                    console.error('Error sending axis request:', err);
                    status.textContent = `Error: ${err.message}`;
                    status.style.background = '#ffebee';
                    status.style.borderColor = '#f44336';
                });
        }

        function updateStatus(message, isActive = false) {
            status.textContent = message;
            status.style.background = isActive ? '#fff3e0' : '#e8f5e9';
            status.style.borderColor = isActive ? '#ff9800' : '#4CAF50';
        }

        // Keyboard handling
        function updateKeyStatus(keyCode, pressed) {
            const device = deviceSelect.value;
            updateStatus(`Device ${device}: Key 0x${keyCode.toString(16).toUpperCase().padStart(2, '0')} ${pressed ? 'PRESSED' : 'RELEASED'}`, pressed);
        }

        keys.forEach(key => {
            // Mouse down - key press
            key.addEventListener('mousedown', (e) => {
                e.preventDefault();
                const keyCode = parseInt(key.dataset.key);
                const device = deviceSelect.value;
                
                if (!pressedKeys.has(keyCode)) {
                    pressedKeys.add(keyCode);
                    key.classList.add('pressed');
                    sendAxisRequest(device, keyCode, 1);
                    updateKeyStatus(keyCode, true);
                }
            });

            // Mouse up - key release
            key.addEventListener('mouseup', (e) => {
                e.preventDefault();
                const keyCode = parseInt(key.dataset.key);
                const device = deviceSelect.value;
                
                if (pressedKeys.has(keyCode)) {
                    pressedKeys.delete(keyCode);
                    key.classList.remove('pressed');
                    sendAxisRequest(device, keyCode, 0);
                    updateKeyStatus(keyCode, false);
                }
            });

            // Mouse leave - release if pressed
            key.addEventListener('mouseleave', (e) => {
                const keyCode = parseInt(key.dataset.key);
                const device = deviceSelect.value;
                
                if (pressedKeys.has(keyCode)) {
                    pressedKeys.delete(keyCode);
                    key.classList.remove('pressed');
                    sendAxisRequest(device, keyCode, 0);
                    updateKeyStatus(keyCode, false);
                }
            });

            // Prevent context menu
            key.addEventListener('contextmenu', (e) => {
                e.preventDefault();
            });
        });

        // Release all keys when mouse leaves window
        document.addEventListener('mouseleave', () => {
            pressedKeys.forEach(keyCode => {
                const device = deviceSelect.value;
                sendAxisRequest(device, keyCode, 0);
                keys.forEach(key => {
                    if (parseInt(key.dataset.key) === keyCode) {
                        key.classList.remove('pressed');
                    }
                });
            });
            pressedKeys.clear();
        });

        // Handle mouseup globally to catch releases outside keys
        document.addEventListener('mouseup', () => {
            const device = deviceSelect.value;
            pressedKeys.forEach(keyCode => {
                sendAxisRequest(device, keyCode, 0);
                keys.forEach(key => {
                    if (parseInt(key.dataset.key) === keyCode) {
                        key.classList.remove('pressed');
                    }
                });
            });
            pressedKeys.clear();
        });

        // Mouse button handling
        const mouseButtons = document.querySelectorAll('.mouse-section .gamepad-button');
        mouseButtons.forEach(button => {
            button.addEventListener('mousedown', (e) => {
                e.preventDefault();
                const buttonId = parseInt(button.dataset.button);
                const device = deviceSelect.value;
                button.classList.add('active');
                sendAxisRequest(device, buttonId, 1);
                updateStatus(`Device ${device}: Mouse Button ${buttonId} PRESSED`, true);
            });

            button.addEventListener('mouseup', (e) => {
                e.preventDefault();
                const buttonId = parseInt(button.dataset.button);
                const device = deviceSelect.value;
                button.classList.remove('active');
                sendAxisRequest(device, buttonId, 0);
                updateStatus(`Device ${device}: Mouse Button ${buttonId} RELEASED`, false);
            });

            button.addEventListener('mouseleave', (e) => {
                const buttonId = parseInt(button.dataset.button);
                const device = deviceSelect.value;
                if (button.classList.contains('active')) {
                    button.classList.remove('active');
                    sendAxisRequest(device, buttonId, 0);
                    updateStatus(`Device ${device}: Mouse Button ${buttonId} RELEASED`, false);
                }
            });
        });

        // Mouse axis handling (movement and wheels)
        const mouseSliders = document.querySelectorAll('.mouse-section .slider');
        mouseSliders.forEach(slider => {
            const valueDisplay = document.getElementById(`${slider.id}-value`);
            
            slider.addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                const axis = parseInt(slider.dataset.axis);
                const device = deviceSelect.value;
                
                valueDisplay.textContent = value;
                sendAxisRequest(device, axis, value);
                
                const axisNames = {
                    5: 'X Left',
                    6: 'X Right',
                    7: 'Y Up',
                    8: 'Y Down',
                    9: 'Wheel Down',
                    10: 'Wheel Up',
                    11: 'H-Wheel Left',
                    12: 'H-Wheel Right'
                };
                
                updateStatus(`Device ${device}: ${axisNames[axis]} = ${value}`, value > 0);
            });
        });

        // Gamepad handling - all sliders now
        const gamepadSliders = document.querySelectorAll('.gamepad-section .slider');

        // Gamepad slider handling (buttons 0-1, axes 0-1000)
        gamepadSliders.forEach(slider => {
            const valueDisplay = document.getElementById(`${slider.id}-value`);
            
            slider.addEventListener('input', (e) => {
                const sliderValue = parseFloat(e.target.value);
                const axis = parseInt(slider.dataset.axis);
                const device = deviceSelect.value;
                
                // For buttons (axes 0-15), convert 0-1 to 0-1000
                // For other axes (16+), they're already 0-1000
                let sendValue;
                if (axis >= 0 && axis <= 15) {
                    // Button: convert 0-1 to 0-1000
                    sendValue = Math.round(sliderValue * 1000);
                    valueDisplay.textContent = sliderValue.toFixed(2);
                } else {
                    // Axis: already 0-1000
                    sendValue = Math.round(sliderValue);
                    valueDisplay.textContent = sendValue;
                }
                
                sendAxisRequest(device, axis, sendValue);
                
                const axisNames = {
                    0: 'D-Pad Up',
                    1: 'D-Pad Down',
                    2: 'D-Pad Left',
                    3: 'D-Pad Right',
                    4: 'Start',
                    5: 'Back',
                    6: 'Left Stick Click',
                    7: 'Right Stick Click',
                    8: 'Left Bumper',
                    9: 'Right Bumper',
                    10: 'Guide',
                    12: 'A',
                    13: 'B',
                    14: 'X',
                    15: 'Y',
                    16: 'Left Trigger',
                    17: 'Right Trigger',
                    18: 'Left Stick X Left',
                    19: 'Left Stick X Right',
                    20: 'Left Stick Y Down',
                    21: 'Left Stick Y Up',
                    22: 'Right Stick X Left',
                    23: 'Right Stick X Right',
                    24: 'Right Stick Y Down',
                    25: 'Right Stick Y Up'
                };
                
                updateStatus(`Device ${device}: ${axisNames[axis]} = ${axis <= 15 ? sliderValue.toFixed(2) : sendValue}`, false);
            });
        });

        // HID Gamepad HAT button handling
        const hidHatButtons = document.querySelectorAll('[data-hidhat]');
        hidHatButtons.forEach(button => {
            button.addEventListener('mousedown', (e) => {
                e.preventDefault();
                const hatDir = button.dataset.hidhat;
                const device = deviceSelect.value;
                button.classList.add('active');
                
                // HAT codes: up=200, down=201, left=202, right=203
                const hatMap = { 'up': 200, 'down': 201, 'left': 202, 'right': 203 };
                const hatCode = hatMap[hatDir];
                
                // Send HAT button press (value 1)
                sendAxisRequest(device, hatCode, 1);
                updateStatus(`Device ${device}: HAT ${hatDir.toUpperCase()} PRESSED`, true);
            });

            button.addEventListener('mouseup', (e) => {
                e.preventDefault();
                const hatDir = button.dataset.hidhat;
                const device = deviceSelect.value;
                button.classList.remove('active');
                
                // Send HAT button release (value 0)
                const hatMap = { 'up': 200, 'down': 201, 'left': 202, 'right': 203 };
                const hatCode = hatMap[hatDir];
                sendAxisRequest(device, hatCode, 0);
                updateStatus(`Device ${device}: HAT ${hatDir.toUpperCase()} RELEASED`, false);
            });

            button.addEventListener('mouseleave', (e) => {
                const hatDir = button.dataset.hidhat;
                const device = deviceSelect.value;
                if (button.classList.contains('active')) {
                    button.classList.remove('active');
                    const hatMap = { 'up': 200, 'down': 201, 'left': 202, 'right': 203 };
                    const hatCode = hatMap[hatDir];
                    sendAxisRequest(device, hatCode, 0);
                    updateStatus(`Device ${device}: HAT ${hatDir.toUpperCase()} RELEASED`, false);
                }
            });
        });

        // HID Gamepad button handling
        const hidButtons = document.querySelectorAll('[data-hidbutton]');
        hidButtons.forEach(button => {
            button.addEventListener('mousedown', (e) => {
                e.preventDefault();
                const buttonId = parseInt(button.dataset.hidbutton);
                const device = deviceSelect.value;
                button.classList.add('active');
                
                // Button codes are 0-31 (direct button indices)
                sendAxisRequest(device, buttonId, 1);
                updateStatus(`Device ${device}: Button ${buttonId + 1} PRESSED`, true);
            });

            button.addEventListener('mouseup', (e) => {
                e.preventDefault();
                const buttonId = parseInt(button.dataset.hidbutton);
                const device = deviceSelect.value;
                button.classList.remove('active');
                
                sendAxisRequest(device, buttonId, 0);
                updateStatus(`Device ${device}: Button ${buttonId + 1} RELEASED`, false);
            });

            button.addEventListener('mouseleave', (e) => {
                const buttonId = parseInt(button.dataset.hidbutton);
                const device = deviceSelect.value;
                if (button.classList.contains('active')) {
                    button.classList.remove('active');
                    sendAxisRequest(device, buttonId, 0);
                    updateStatus(`Device ${device}: Button ${buttonId + 1} RELEASED`, false);
                }
            });
        });

        // HID Gamepad axis handling
        const hidAxisSliders = document.querySelectorAll('[data-hidaxis]');
        hidAxisSliders.forEach(slider => {
            const valueDisplay = document.getElementById(`${slider.id}-value`);
            
            slider.addEventListener('input', (e) => {
                const value = parseInt(e.target.value);
                const axisId = parseInt(slider.dataset.hidaxis);
                const device = deviceSelect.value;
                
                valueDisplay.textContent = value;
                
                // Axis codes: LX=100, LY=101, RX=102, RY=103, LT=104, RT=105
                // Scale 0-255 to 0-1000 for setAxis API
                const scaledValue = Math.round((value * 1000) / 255);
                sendAxisRequest(device, 100 + axisId, scaledValue);
                
                const axisNames = {
                    0: 'Left Stick X',
                    1: 'Left Stick Y',
                    2: 'Right Stick X',
                    3: 'Right Stick Y',
                    4: 'Left Trigger (Z)',
                    5: 'Right Trigger (Rz)'
                };
                
                updateStatus(`Device ${device}: ${axisNames[axisId]} = ${value}`, false);
            });
        });

        // Update status on device change
        deviceSelect.addEventListener('change', () => {
            const type = typeSelect.value;
            status.textContent = `Switched to Device ${deviceSelect.value} (${type})`;
        });
    </script>
</body>
</html>
)html";
    
    return html.str();
}

void initWebserver(Main2Pico& picoRpcClient) {
    std::cout << "Starting HID Web Tester server on port 8080..." << std::endl;
    
    std::thread httpThread([&picoRpcClient]() {
        httplib::Server svr;

        // Main page - serve the HTML interface
        svr.Get("/", [](const httplib::Request &, httplib::Response &res) {
            res.set_content(generateHtml(), "text/html");
        });
        
        svr.Get("/rebootflashmode", [&picoRpcClient](const httplib::Request &req, httplib::Response &res) {
            picoRpcClient.rebootFlashMode();
            res.set_content("OK", "text/plain");
        });
        // Endpoint to set axis/button values
        // Query params: device (1-10), axis (key code), value (0 or 1)
        svr.Get("/setaxis", [&picoRpcClient](const httplib::Request &req, httplib::Response &res) {
            // Parse parameters
            if (req.has_param("device") && req.has_param("axis") && req.has_param("value")) {
                int device = std::stoi(req.get_param_value("device"));
                int axis = std::stoi(req.get_param_value("axis"));
                int value = std::stoi(req.get_param_value("value"));
                
                std::cout << "[WEBTESTER] Device " << device 
                          << " Axis 0x" << std::hex << axis << std::dec
                          << " Value " << value << std::endl;
                picoRpcClient.setAxis(device, axis, value);
                // TODO: Call RPC method to send key press/release to Pico
                // For now, just acknowledge the request
                // Example: picoRpcClient.sendKeyPress(device, axis, value);
                
                res.set_content("OK", "text/plain");
            } else {
                res.status = 400;
                res.set_content("Missing parameters", "text/plain");
            }
        });

        std::cout << "HID Web Tester started at http://0.0.0.0:8080" << std::endl;
        svr.listen("0.0.0.0", 8080);
    });
    
    httpThread.detach();
}
