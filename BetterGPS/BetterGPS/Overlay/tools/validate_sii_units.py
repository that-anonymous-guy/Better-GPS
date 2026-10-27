"""Validate SCS unit-name tokens and local UI links, not opaque image/text strings.

SCS rule: https://modding.scssoft.com/wiki/Documentation/Engine/Units
Some third-party SII parsers accept long names which ATS rejects at load time.
"""
import argparse
from pathlib import Path
import re

def valid_unit_name(name):
    return bool(name) and all(re.fullmatch(r'[a-z0-9_]{1,12}',part) for part in (name[1:] if name.startswith('.') else name).split('.'))

def validate_sii_units(text):
    units=set()
    for match in re.finditer(r'^\s*[a-z0-9_]+(?:::[a-z0-9_]+)*\s*:\s*([._a-zA-Z0-9]+)\s*\{',text,re.M):
        name=match[1]
        if not valid_unit_name(name):raise ValueError(f'Invalid SCS unit name {name!r}: each component must be 1..12 lowercase letters/digits/underscores')
        if name in units:raise ValueError(f'Duplicate unit name {name}')
        units.add(name)
    if not units:raise ValueError('No SII units found')
    for match in re.finditer(r'^\s*(?:my_parent|my_children\[\d+\])\s*:\s*([._a-zA-Z0-9]+)\s*$',text,re.M):
        if match[1]!='null' and match[1] not in units:raise ValueError(f'Unresolved local UI link {match[1]}')
    return len(units)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('files',nargs='+',type=Path);args=parser.parse_args()
    for file in args.files:print(f'PASS: SCS token limits/local UI links, {validate_sii_units(file.read_text(encoding="utf-8-sig"))} units: {file}')
