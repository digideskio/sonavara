import sys
from setuptools import find_packages
from setuptools import setup
from setuptools.command.test import test as TestCommand


class PyTest(TestCommand):
    user_options = [('pytest-args=', 'a', 'Arguments to pass to py.test')]

    def initialize_options(self):
        super().initialize_options()
        self.pytest_args = []

    def finalize_options(self):
        super().finalize_options()
        self.test_args = []
        self.test_suite = True

    def run_tests(self):
        import pytest
        sys.exit(pytest.main(self.pytest_args))

setup(
    name="sonavara",
    description="?",
    author="Yuki Izumi",
    author_email="yuki@kivikakk.ee",
    license="kindest",
    version="0.1",
    entry_points={"console_scripts": ["sonavara = sonavara.main:main"]},
    install_requires=[],
    tests_require=['pytest'],
    cmdclass={'test': PyTest},
    zip_safe=True,
    packages=find_packages(),
    package_data={
        'sonavara': ['c/tokeniser.c', 'c/nfa.c', 'c/engine.c', 'c/lexer.c'],
    },
)
