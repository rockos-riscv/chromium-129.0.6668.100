---
breadcrumbs:
- - /getting-involved
  - Getting Involved
page_name: bug-triage
title: Triaging Bugs
---

You don’t have to be a programmer to contribute to the Chromium project. There
are always new bugs coming in, and those bugs need to be clarified, confirmed,
and routed to the right people before process of getting them fixed can even
begin.
If you are interested in helping with this important part of the project, here
are a few ways you can get started. Everything listed here except the parts in
italics and the "Categorizing bugs" section is something anyone can jump in and
start doing. Once you have been triaging for a while, you may want to consider
[getting permissions to edit bugs](/getting-involved/get-bug-editing-privileges)
so that you can do even more.

[TOC]

## Finding duplicates

Many of the bugs that are filed are duplicates of existing bugs. Look through
the [unconfirmed bugs](https://issues.chromium.org/hotlists/5437934)
for any that are already filed.

*   If you can edit bugs: Close the new bug as a duplicate of the
            existing bug. If there are several new duplicates of each other with
            no established bug, use the one with the most information.
*   If you can’t edit bugs, leave a comment with the link to the existing bug.
          Be sure to say “bug 12345” or “issue 12345” instead of just the number,
          so the bug system will make it a link.

The more you triage bugs, the more duplicates you will recognize immediately. If
you suspect something might be a duplicate but aren’t sure (such as with a
feature request that seems likely to have been made before), search for as many
of the variants of the key words in the bug as you can think of.

## Clarifying and/or reproducing bugs

Knowing how to reproduce a bug is the first step toward fixing it. The bug
template encourages people to provide steps to reproduce a bug, but sometimes
the steps are missing, unclear, or just don’t work for everyone. Note that if
something is clearly a feature request, rather than a bug report, you should
skip this step.

Look through the [unconfirmed bugs](https://issues.chromium.org/hotlists/5437934)
for reports that nobody has tried to reproduce.

*   If you can reproduce the bug:
    *   Leave a comment saying so, and include your Chromium version and
                OS.
    *   If the steps were unclear, it’s helpful to leave more detailed
                steps.
    *   If you weren’t testing with the latest version of Chromium, try
                there, and see if it’s still reproducible, and add that
                information to your comment as well.
    *   If the bug is with web content, and you have access to browsers
                that the reporter didn’t test, try with those browsers and
                report your results.
    *   If you can edit bugs, and have tried to make sure that it's not a duplicate
                and it's in the [Unconfirmed hotlist](https://g-issues.chromium.org/savedsearches/6680973)
                move it to the [Available hotlist](https://g-issues.chromium.org/savedsearches/6681397)
*   If you feel that the steps were clear but you couldn’t reproduce the
            bug:
    *   Leave a comment saying that you couldn’t reproduce it, including
                your Chromium version and OS.
        *   **Note:** If you are using an older version of Chromium than
                    the reporter, you should do this only if you **can**
                    reproduce it with a newer version (indicating a regression).
    *   If you were using a different version of Chromium, and have
                access to the version it was reported with, try with that
                version.
    *   If the reporter doesn’t say what version they are using, ask for
                that information. If the bug is related to OS-specific code
                (browser chrome, plugins, etc.), ask for their OS if it’s not in
                the report.
    *   If there is any other clarifying information that you think
                would help, ask for that.
*   If the steps and/or results are missing or unclear, so that you
            can’t tell either what the bug is, or how to reproduce it:
    *   Ask for details. Be specific about what you need to know, as
                people often leave out steps without realizing it because
                something is obvious to them.
    *   If it would be helpful, ask for a screenshot of the bug.
    *   If you can edit bugs: Add the [Needs-Feedback hotlist](https://g-issues.chromium.org/issues?q=hotlistid:5433459%20status:open).

## Cleaning up old bugs

While it’s most important to get new bugs triaged, as they are most likely to be
relevant, there are also a number of old bugs that nobody has had time to follow
up on. It can be helpful to go through old unconfirmed bugs to weed out the ones
that aren’t relevant any more. All the advice above applies, but there are a few
extra things to consider with old bugs:

*   If you can’t reproduce the bug, ask the user if they can still
            reproduce the bug with a current version of Chromium.
*   For feature requests, if the request is about feature that has
            changed enough that the request no longer applies, add a comment
            saying so.
    *   If you can edit bugs: Close the bug as "Won't Fix (Obsolete)"
*   If the bug doesn’t contain enough information to be confirmed and
            someone has asked for clarifying information that was never
            provided, politely ask again for that information.
    *   If you can edit bugs:
        *   If the bug has been waiting for information for over a
                    month, and there has already been at least one reminder, add
                    a comment saying that the bug is being closed due to lack of
                    response and that a new bug should be filed with the
                    requested information if it’s still reproducible, then close
                    the bug as "Won't Fix (Obsolete)".
        *   Otherwise, add the Needs-Feedback hotlist if it’s not already there

## Creating reduced test cases

For bugs that are about compatibility with specific sites, creating a reduced
test case is extremely helpful. If you are familiar with HTML, CSS, and/or
Javascript, this is a great way to use your skills to help improve Chromium.

*   If the problem is that the site is doing something wrong:
    *   Explain in detail what is causing the problem.
    *   Suggest that the bug reporter contact the site to let them know
                about the problem.
*   If the problem is due to a bug in Chromium:
    *   Explain in detail what is isn’t working right.
    *   If possible, attach a minimal test case that demonstrates the
                underlying bug.
    *   If possible, test with Safari as well to see if the bug happens
                there, and report your results
    *   If you can edit bugs: change the component field to Blink.

## Categorizing bugs

Once you can [edit bugs](/getting-involved/get-bug-editing-privileges), giving
bugs the correct labels helps them move through the triage process faster. There
are a lot of hotlists, here are the most important for incoming bugs:

*   If the bug has no Components, please assign it to a chromium component:
    *   Blink == Web Content
    *   Internals == Internals (Network stack, plugins, etc...)
    *   Platform == Chrome's Developer Platform and Tools (Ext, AppsV2,
                NaCl, DevTools)
    *   UI&gt;Shell == ChromeOS Shell/ Window Manager
    *   UI&gt;Browser == Browser
    *   OS&gt;Hardware
    *   OS&gt;Kernel
    *   OS&gt;Systems
    *   If the bug/feature is about specific web technology, add the
                appropriate Blink&gt;\* sub component instead.
    *   Try to find the best match; if it’s borderline, use your best
                guess.
*   If the bug is about a crash or hang, add the **Stability-Crash** or
            **Stability-Hang** hotlist.
*   If the bug is about a language translation, add the
            **UI&gt;Localization** component.