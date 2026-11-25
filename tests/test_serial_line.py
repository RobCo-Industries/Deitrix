"""serial_line: a test reads only lines that have fully arrived.

The serial buffer can end in the middle of a line. An "about" line cut
there once made test_stat_walk fail with a ValueError in about_fields.
"""

import re

import pytest

from conftest import serial_line
from test_stat import ABOUT, about_fields

FULL = "deitrix: stat about Deitrix=0.1.0 | Pip-OS=1.2.0 | ABI=1.3\r\n"
CUT = "deitrix: stat shows STATUS\r\ndeitrix: stat about Deitrix=0.1.0 | Pip-O"


def test_end_of_buffer_pattern_reads_a_cut_line():
    # The pattern the tests used before: "$" matches at the buffer's end.
    old = re.compile(r"^deitrix: stat about (.*?)\r?$", re.M)
    [line] = old.findall(CUT)
    with pytest.raises(ValueError):
        about_fields(line)


def test_cut_line_is_not_read():
    assert ABOUT.findall(CUT) == []


def test_line_is_read_once_complete():
    assert ABOUT.findall(CUT + "S=1.2.0 | ABI=1.3\n") == ["Deitrix=0.1.0 | Pip-OS=1.2.0 | ABI=1.3"]
    assert about_fields(ABOUT.findall(FULL)[0])["ABI"] == "1.3"


def test_cut_word_is_not_read():
    tab = serial_line(r"deitrix: tab=(\w+)")
    assert tab.findall("deitrix: tab=RA") == []
    assert tab.findall("deitrix: tab=RADIO\n") == ["RADIO"]
