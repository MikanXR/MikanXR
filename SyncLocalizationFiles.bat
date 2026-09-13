@echo off

echo "Sync new strings from English source"
python tools/localization.py sync

echo "Verify string data"
python tools/localization.py check