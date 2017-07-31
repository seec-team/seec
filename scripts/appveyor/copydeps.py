import os
import subprocess
import shutil

OBJDUMP = "objdump"
DLL_NAME_PREFIX = 'DLL Name:'

paths = filter(lambda line: not "windows" in line.lower(),
               os.environ['PATH'].split(os.pathsep))

def find_in_path(file):
  for path in paths:
    fullpath = os.path.join(path, file)
    if os.path.isfile(fullpath):
      return fullpath
  return None

def in_cwd(file):
  return os.path.isfile(os.path.join(os.getcwd(), file))

def get_dependencies(file):
  try:
    for line in subprocess.check_output([OBJDUMP, '-p', file]).split('\n'):
      if DLL_NAME_PREFIX in line:
        yield line[line.find(DLL_NAME_PREFIX) + len(DLL_NAME_PREFIX):].strip()
  except CalledProcessError:
    print "exception checking " + file
    pass

def is_interesting_local_file(file):
  lwr = file.lower()
  return os.path.isfile(file) and (lwr.endswith("exe") or lwr.endswith("dll"))

searchfiles = [f for f in os.listdir('.') if is_interesting_local_file(f)]

while len(searchfiles):
  localfile = searchfiles.pop(0)
  print "checking " + localfile
  for dep in get_dependencies(localfile):
    if not in_cwd(dep):
      fullpath = find_in_path(dep)
      if fullpath:
        print "copying from " + fullpath
        shutil.copy(fullpath, os.getcwd())
        searchfiles.append(dep)
