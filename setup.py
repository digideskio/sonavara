from setuptools import setup

setup(
    name="sonavara",
    description="?",
    author="Yuki Izumi",
    author_email="yuki@kivikakk.ee",
    license="kindest",
    version="0.1",
    packages=["sonavara"],
    entry_points={"console_scripts": ["sonavara = sonavara.main:main"]},
    install_requires=[],
)
