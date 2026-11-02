"""一键跑完所有主机侧测试。用法: python tests/run_all.py"""
import os, sys, subprocess

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
PY = sys.executable

TESTS = ['test_tree_contract.py', 'test_token_equivalence.py',
         'test_project_consistency.py', 'test_negative_control.py',
         'test_coff_section_bytes.py',
         'test_difftest.py', 'test_difftest_negctl.py', 'test_build_evidence.py']

rc = 0
for t in TESTS:
    print('=' * 64)
    print('== ' + t)
    print('=' * 64)
    r = subprocess.run([PY, os.path.join(HERE, t)], cwd=ROOT)
    if r.returncode != 0:
        rc = 1
        print('>> FAILED: %s' % t)
print('=' * 64)
print('ALL TESTS PASSED' if rc == 0 else 'SOME TESTS FAILED')
sys.exit(rc)
