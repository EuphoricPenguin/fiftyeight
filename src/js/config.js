module.exports = [
  {
    "type": "heading",
    "defaultValue": "fiftyeight Configuration"
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Display Settings"
      },
      {
        "type": "toggle",
        "messageKey": "DarkMode",
        "label": "Dark Mode",
        "defaultValue": false,
        "description": "Enable dark mode (white text on black background)"
      },
      {
        "type": "toggle",
        "messageKey": "ShowAmPm",
        "label": "Show AM/PM Indicator",
        "defaultValue": false,
        "description": "Display AM/PM indicator in top left corner instead of the month date"
      },
      {
        "type": "toggle",
        "messageKey": "Use24HourFormat",
        "label": "24-Hour Time Format",
        "defaultValue": false,
        "description": "Use 24-hour format instead of 12-hour format"
      },
      {
        "type": "toggle",
        "messageKey": "UseTwoLetterDay",
        "label": "Two-Letter Day Abbreviations",
        "defaultValue": false,
        "description": "Use 2-letter day abbreviations instead of 3-letter"
      },
      {
        "type": "toggle",
        "messageKey": "DebugMode",
        "label": "Debug Mode",
        "defaultValue": false,
        "description": "Enable debug mode to cycle through all graphics for evaluation"
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
