module.exports = [
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Widget Configuration"
      },
      {
        "type": "select",
        "messageKey": "TopLeftWidget",
        "label": "Top Left Corner",
        "defaultValue": "1",
        "description": "Select widget to display in top left corner",
        "options": [
          {
            "label": "Month Date",
            "value": "1"
          },
          {
            "label": "Day Date",
            "value": "2"
          },
          {
            "label": "AM/PM Indicator",
            "value": "3"
          },
          {
            "label": "Battery Indicator",
            "value": "4"
          },
          {
            "label": "Step Counter",
            "value": "5"
          },
          {
            "label": "None",
            "value": "0"
          }
        ]
      },
      {
        "type": "select",
        "messageKey": "TopRightWidget",
        "label": "Top Right Corner",
        "defaultValue": "2",
        "description": "Select widget to display in top right corner",
        "options": [
          {
            "label": "Month Date",
            "value": "1"
          },
          {
            "label": "Day Date",
            "value": "2"
          },
          {
            "label": "AM/PM Indicator",
            "value": "3"
          },
          {
            "label": "Battery Indicator",
            "value": "4"
          },
          {
            "label": "Step Counter",
            "value": "5"
          },
          {
            "label": "None",
            "value": "0"
          }
        ]
      },
      {
        "type": "input",
        "messageKey": "StepGoal",
        "label": "Step Goal",
        "defaultValue": "10000",
        "description": "Daily step goal for progress bar (default: 10000)",
        "attributes": {
          "type": "number",
          "min": "1000",
          "max": "50000",
          "step": "1000"
        }
      }
    ]
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
        "messageKey": "ShowSecondDot",
        "label": "Show Second Dot",
        "defaultValue": true,
        "description": "Show the second dot complication in the background"
      },
      {
        "type": "toggle",
        "messageKey": "ShowHourMinuteDots",
        "label": "Show Hour and Minute Dots",
        "defaultValue": true,
        "description": "Show the hour and minute dot complications in the background"
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Other Stuff"
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
