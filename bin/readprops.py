import configparser
import subprocess
import os
run_number = os.getenv('GITHUB_RUN_NUMBER', '0')
build_location = os.getenv('BUILD_LOCATION', 'local')

def readProps(prefsLoc):
    """Read the version of our project as a string"""

    config = configparser.RawConfigParser()
    config.read(prefsLoc)
    version = dict(config.items("VERSION"))

    rev = version.get("rev", version.get("revision", "")).strip()
    if rev:
        short_ver = "{}.{}.{}-{}".format(
            version["major"], version["minor"], version["build"], rev
        )
    else:
        short_ver = "{}.{}.{}".format(
            version["major"], version["minor"], version["build"]
        )

    verObj = dict(
        short=short_ver,
        long="unset",
        deb="unset",
    )

    # Try to find current build SHA if the workspace is clean. This could fail if git is not installed
    try:
        is_release = os.getenv("RELEASE_BUILD", "0") in (
            "1",
            "true",
            "True",
        ) or os.getenv("GITHUB_REF_TYPE") == "tag"

        # Pin abbreviation length to keep local builds and CI matching (avoid auto-shortening)
        sha = (
            subprocess.check_output(["git", "rev-parse", "--short=7", "HEAD"])
            .decode("utf-8")
            .strip()
        )

        if is_release:
            verObj["long"] = verObj["short"]
        else:
            # MyNodeInfo.firmware_version max_size is 18 (17 characters + \0) in protobuf
            max_len = 17
            rem = max_len - len(verObj["short"]) - 1
            if rem >= 4:
                suffix = sha[: min(len(sha), rem)]
                verObj["long"] = "{}.{}".format(verObj["short"], suffix)
            else:
                verObj["long"] = verObj["short"]

        verObj["deb"] = "{}.{}~{}{}".format(
            verObj["short"], run_number, build_location, sha
        )
    except:
        verObj["long"] = verObj["short"]
        verObj["deb"] = "{}.{}~{}".format(
            verObj["short"], run_number, build_location
        )

    return verObj


# print("path is" + ','.join(sys.path))