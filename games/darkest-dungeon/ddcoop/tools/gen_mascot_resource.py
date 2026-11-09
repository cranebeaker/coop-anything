"""Optional local GIF -> generated RC include; image is kept outside published source."""
from pathlib import Path
import sys
source=Path(sys.argv[1]) if sys.argv[1] else None
out=Path(sys.argv[2]);out.parent.mkdir(parents=True,exist_ok=True)
text='// No optional mascot configured.\n'
if source:
    data=source.read_bytes()
    assert 13<=len(data)<=8*1024*1024 and data[:6] in (b'GIF87a',b'GIF89a'),'Expected a bounded GIF file'
    path=source.resolve().as_posix()
    assert '"' not in path and '\n' not in path
    text='201 RCDATA "'+path+'"\n'
out.write_text(text,encoding='utf-8',newline='\n')
