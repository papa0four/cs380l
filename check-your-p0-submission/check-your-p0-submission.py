#!/usr/bin/python3

import argparse
import json
import os
import subprocess
import time

from typing import List
from tqdm import tqdm

    
def get_project_0_submission_files() -> List[str]:
    """Get the list of expected submission files for project 0."""
    # Please submit a single zip file. Your zip file should contain 2 files as
    # follows:
    #
    # <UT EID>-submission.zip
    #
    # wish.c
    # pintos.patch
    #
    return [
        "wish.c",
        "pintos.patch"
    ]

# Returns True on a successful check of project submission.
# Otherwise, returns False.
def do_check_project_files(dir: str, files: List[str], logger) -> bool:
    """
    Returns True on a succesful check of project files. Otherwise
    returns False.
    """
    
    # Find all files matching "name" in the "path"
    def find_all(name: str, path: str) -> List[str]:
        result = []
        for root, _, files in os.walk(path):
            if name in files:
                result.append(os.path.join(root, name))
        return result

    files_missing = []
    files_duped = []
    for f in files:
        result = find_all(f, dir)

        # Error, if no results were found
        if len(result) == 0:
            files_missing.append(f)
        # Error, if more than one result was found
        elif len(result) > 1:
            files_duped.append(f)

    def list2str(l: List[str]) -> str:
        s = ""
        for e in l:
            s += e + ","
        return s.rstrip(',')

    def strstr2str(miss: str, duped: str) -> str:
        s = ""
        if len(miss) > 0:
            s += "Missing file(s) "
            s += miss
        if len(duped) > 0:
            if len(s) > 0:
                s += " AND "
            s += "Duplicated file(s) "
            s += duped
        return s
    
    result_str = strstr2str(
        miss=list2str(files_missing),
        duped=list2str(files_duped)
    )
    if len(result_str) > 0:
        if logger is not None:
            logger.write(f"[ERROR] {result_str}")
        return False
    else:
        if logger is not None:
            logger.write("[PASS] Your submission has the necessary files.")
        return True
        
def do_check_pintos_patch(pdir: str, pfile: str, timeout: int, logger):
    """Patch the Pintos project submission and return the result of doing so."""
    
    assert os.path.exists(pdir), f"Error: Invalid directory - {pdir}"
    assert os.path.exists(pfile), f"Error: Invalid patch file - {pfile}"

    # Copy over fresh aos_pintos codebase
    command = ['cp', 'aos_pintos', '-r', 'check-submission']    
    try:
        subprocess.check_output(
            [
                'timeout', '--preserve-status', '-s', 'INT', str(timeout)
            ] + command,
            stderr=subprocess.STDOUT,
            cwd=f"{pdir}/.."
        )
    except subprocess.CalledProcessError:
        if logger is not None:
            logger.write("[FAIL] There was a problem with copying fresh aos_pintos.")
        return

    # Try: patch -p0 -i pintos.patch
    patch_command = ['patch', '-p0', '-i', os.path.abspath(pfile)]
    try:
        subprocess.check_output(
            [
                'timeout', '--preserve-status', '-s', 'INT', str(timeout)
            ] + patch_command,
            stderr=subprocess.STDOUT,
            cwd=pdir
        )
    except subprocess.CalledProcessError:
        if logger is not None:
            logger.write("[FAIL] There was a problem with patching.")
        return
    
    if logger is not None:
        print("[PASS] Patch was successful.")


def do_check_submission(args):
    if args.verbose:
        print(f"Check submission for Project 0")
    files = get_project_0_submission_files()
    if do_check_project_files(dir=args.dir, files=files, logger=tqdm if args.verbose else None):
        do_check_pintos_patch(
            pdir=args.dir,
            pfile=f"{args.dir}/pintos.patch",
            timeout=20,
            logger=tqdm if args.verbose else None)
        

if __name__ == '__main__':
    parser = argparse.ArgumentParser()

    parser.add_argument(
        '-d', '--dir', default='check-submission', type=str,
        help='Top-level directory containing student(s) submission'
    )
    parser.add_argument(
        '-v', '--verbose', action='store_true',
        help='Run with verbosity'
    )

    args = parser.parse_args()
    print(json.dumps(vars(args), indent=4))
    for i in tqdm(range(1,10), desc="Pre-check arguments"):
        time.sleep(1)
    do_check_submission(args)
