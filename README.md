This is an **unofficial** clone of the FreeRTOS project's [Subversion repository hosted on SourceForge](https://svn.code.sf.net/p/freertos/code/).  You probably shouldn't depend on this repository being available for you to use.

`git-svn` config (example .git/config)

    [svn-remote "svn"]
        url = https://svn.code.sf.net/p/freertos/code
        fetch = trunk:refs/remotes/trunk
        branches = branches/*:refs/remotes/*
        tags = tags/*:refs/remotes/tags/*


## Updating from Subversion

README branch contains the README.md file changes.
For your copies:
1. `git checkout vendor`
2. `git svn fetch`
3. `git merge trunk`

-> I do the same with master branch.
1. `git checkout master`
2. `git svn fetch`
3. `git merge trunk`
