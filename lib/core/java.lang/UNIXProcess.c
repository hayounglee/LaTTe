/*
 * java.lang.UNIXProcess.c
 *
 * Copyright (c) 1996-7 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "lib-license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.demon.co.uk>
 *
 * Modified by MASS Laboratory, SNU, 1999
 */

#define	DBG(s)

#include "config.h"
#include "config-std.h"
#include "config-mem.h"
#include "config-io.h"
#include "config-signal.h"
#include "files.h"
#include "java.io.stubs/FileDescriptor.h"
#include "java.lang.stubs/UNIXProcess.h"
#include "runtime/support.h"
#include <sys/types.h>
#include <sys/wait.h>

void
java_lang_UNIXProcess_exec(struct Hjava_lang_UNIXProcess* this, HArrayOfObject* args, HArrayOfObject* envs)
{
	char** argv;
	char** arge;
	int i;
	char* path;
	int arglen;
	int envlen;

	arglen = (args ? obj_length(args) : 0);
	envlen = (envs ? obj_length(envs) : 0);

DBG(	printf("args %d envs %d\n", arglen, envlen); fflush(stdout);	)

	if (arglen < 1) {
		SignalError(0, "java.io.IOException", "No such file");
	}
	path = makeCString((struct Hjava_lang_String*)unhand(args)->body[0]);
	/* Check program exists and we can execute it */
        i = access(path, X_OK);
	if (i < 0) {
		free(path);
		SignalError(0, "java.io.IOException", SYS_ERROR);
	}

	/* Build arguments and environment */
	argv = calloc(arglen + 1, sizeof(char*));
	arge = calloc(envlen + 1, sizeof(char*));
	for (i = 0; i < arglen; i++) {
		argv[i] = makeCString((struct Hjava_lang_String*)unhand(args)->body[i]);
	}
	for (i = 0; i < envlen; i++) {
		arge[i] = makeCString((struct Hjava_lang_String*)unhand(envs)->body[i]);
	}
	/* Execute program */
#if defined(HAVE_EXECVE)
	execve(path, argv, arge);
#else
	unimp("execve() not provided");
#endif
	exit(-1);
}

jint
java_lang_UNIXProcess_fork(struct Hjava_lang_UNIXProcess* this)
{
	int pid;
	int in[2];
	int out[2];
	int err[2];
	int sync[2];
	char b;

	/* Create the pipes to communicate with the child */
	pipe(in);
	pipe(out);
	pipe(err);
	pipe(sync);

#if defined(HAVE_FORK)
	pid = fork();
#else
	unimp("fork() not provided");
#endif
	switch (pid) {
	case 0:
		/* Child */
		dup2(in[0], 0);
		dup2(out[1], 1);
		dup2(err[1], 2);

		/* What is sync about anyhow?  Well my current guess is that
		 * the parent writes a single byte to it when it's ready to
		 * proceed.  So here I wait until I get it before doing
		 * anything.
		 */
		read(sync[0], &b, 1);

		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
		close(sync[0]);
		close(sync[1]);
		break;
	case -1:
		/* Error */
		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
		close(sync[0]);
		close(sync[1]);
		SignalError(0, "java.io.IOException", "Fork failed");
		break;
	default:
		/* Parent */
		unhand(unhand(this)->stdin_fd)->fd = in[1];
		unhand(unhand(this)->stdout_fd)->fd = out[0];
		unhand(unhand(this)->stderr_fd)->fd = err[0];
		unhand(unhand(this)->sync_fd)->fd = sync[1];
		close(in[0]);
		close(out[1]);
		close(err[1]);
		close(sync[0]);
		unhand(this)->isalive = 1;
		unhand(this)->exit_code = 0;
		unhand(this)->pid = pid;
		break;
	}
	return (pid);
}

jint
java_lang_UNIXProcess_forkAndExec(struct Hjava_lang_UNIXProcess* this, HArrayOfObject* args, HArrayOfObject* envs)
{
	int pid;
	int in[2];
	int out[2];
	int err[2];
	int sync[2];
	char** argv;
	char** arge;
	int i;
	char* path;
	int arglen;
	int envlen;
	char b;

	/* Create the pipes to communicate with the child */
	pipe(in);
	pipe(out);
	pipe(err);
	pipe(sync);

#if defined(HAVE_FORK)
	pid = fork();
#else
	unimp("fork() not provided");
#endif
	switch (pid) {
	case 0:
		/* Child */
		dup2(in[0], 0);
		dup2(out[1], 1);
		dup2(err[1], 2);

		/* What is sync about anyhow?  Well my current guess is that
		 * the parent writes a single byte to it when it's ready to
		 * proceed.  So here I wait until I get it before doing
		 * anything.
		 */
		read(sync[0], &b, 1);

		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
		close(sync[0]);
		close(sync[1]);
		break;
	case -1:
		/* Error */
		close(in[0]);
		close(in[1]);
		close(out[0]);
		close(out[1]);
		close(err[0]);
		close(err[1]);
		close(sync[0]);
		close(sync[1]);
		SignalError(0, "java.io.IOException", "Fork failed");
		return -1;
	default:
		/* Parent */
		unhand(unhand(this)->stdin_fd)->fd = in[1];
		unhand(unhand(this)->stdout_fd)->fd = out[0];
		unhand(unhand(this)->stderr_fd)->fd = err[0];
		unhand(unhand(this)->sync_fd)->fd = sync[1];
		close(in[0]);
		close(out[1]);
		close(err[1]);
		close(sync[0]);
		unhand(this)->isalive = 1;
		unhand(this)->exit_code = 0;
		unhand(this)->pid = pid;
		return (pid);
	}

	arglen = (args ? obj_length(args) : 0);
	envlen = (envs ? obj_length(envs) : 0);

DBG(	printf("args %d envs %d\n", arglen, envlen); fflush(stdout);	)

	if (arglen < 1) {
		SignalError(0, "java.io.IOException", "No such file");
	}
	path = makeCString((struct Hjava_lang_String*)unhand(args)->body[0]);
	/* Check program exists and we can execute it */
        i = access(path, X_OK);
	if (i < 0) {
		free(path);
		SignalError(0, "java.io.IOException", SYS_ERROR);
	}

	/* Build arguments and environment */
	argv = calloc(arglen + 1, sizeof(char*));
	arge = calloc(envlen + 1, sizeof(char*));
	for (i = 0; i < arglen; i++) {
		argv[i] = makeCString((struct Hjava_lang_String*)unhand(args)->body[i]);
	}
	for (i = 0; i < envlen; i++) {
		arge[i] = makeCString((struct Hjava_lang_String*)unhand(envs)->body[i]);
	}
	/* Execute program */
#if defined(HAVE_EXECVE)
	execve(path, argv, arge);
#else
	unimp("execve() not provided");
#endif
	exit(-1);
}


jint
java_lang_UNIXProcess_waitForUNIXProcess(struct Hjava_lang_UNIXProcess* this)
{
	int status;

	if (unhand(this)->isalive == 0) {
#if defined(HAVE_WAITPID)
		if (waitpid(unhand(this)->pid, &status, 0) != -1) {
			unhand(this)->exit_code = status;
			unhand(this)->isalive = 1;
			unhand(this)->exit_code = 0;
		}
#else
		unimp("waitpid() not provided");
#endif
	}
	return (unhand(this)->exit_code);
}

void
java_lang_UNIXProcess_destroy(struct Hjava_lang_UNIXProcess* this)
{
#if defined(HAVE_KILL)
	kill(unhand(this)->pid, SIGTERM);
#else
	unimp("kill() not provided");
#endif
}

void
java_lang_UNIXProcess_run(struct Hjava_lang_UNIXProcess* this)
{
	unimp("java.lang.UNIXProcess:run not implemented"); /* FIXME */
}

void
java_lang_UNIXProcess_notifyReaders(struct Hjava_lang_UNIXProcess* this)
{
	unimp("java.lang.UNIXProcess:notifyReaders not implemented"); /* FIXME */
}
