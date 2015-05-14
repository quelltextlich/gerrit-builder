gerrit-builder
==============

Scripts to build `gerrit` and plugins.



Setup
-----

* Make sure you either have `jdk`, `ant`, and `maven` installed in
  your system.

* Clone the `gerrit-builder` repository by running

    ```
    git clone git://git.quelltextlich.at/gerrit/gerrit-builder
    ```

* Change into the clone's directory by running

    ```
    cd gerrit-builder
    ```

* Tell `gerrit-builder` to fetch the gerrit repo and plugin repos, and
  setup the needed build tools like `watchman`, and `buck`:

    ```
    ./setup.sh
    ```

* To build `gerrit` and the plugins, run

    ```
    ./build.sh
    ```

* You'll find the built artifacts in the directory `artifacts/$DATE`, and if
  you point your browser, to `artifacts/$DATE/index.html`, you'll get a nice
  overview page for the build and the artifacts' status.

* Use

    ```
    ./build.sh --help
    ```

  to see the script's help page.
