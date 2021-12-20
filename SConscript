
from building import *

cwd     = GetCurrentDir()
src     = Glob('*.c')
path    = [cwd]

group = DefineGroup('CW2015', src, depend = ['PKG_USING_CW2015'], CPPPATH = path)

Return('group')
