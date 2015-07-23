/*
 * external_wrappers.h
 * Non-shared library wrapper for tjwassoc.co.uk package.
 *
 * Copyright (c) 1996,97 T. J. Wilkinson & Associates, London, UK.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * Written by Tim Wilkinson <tim@tjwassoc.co.uk>
 */

/* tjwassoc.co.uk/APIcore */

KAFFE_NATIVE(java_lang_ClassLoader_init)
KAFFE_NATIVE(java_lang_ClassLoader_defineClass0)
KAFFE_NATIVE(java_lang_ClassLoader_resolveClass0)
KAFFE_NATIVE(java_lang_ClassLoader_findSystemClass0)
KAFFE_NATIVE(java_lang_ClassLoader_getSystemResourceAsStream0)
KAFFE_NATIVE(java_lang_ClassLoader_getSystemResourceAsName0)
KAFFE_NATIVE(java_lang_Class_forName)
KAFFE_NATIVE(java_lang_Class_newInstance)
/* KAFFE_NATIVE(java_lang_Class_isInstance) */
KAFFE_NATIVE(java_lang_Class_isAssignableFrom)
KAFFE_NATIVE(java_lang_Class_isInterface)
KAFFE_NATIVE(java_lang_Class_isArray)
KAFFE_NATIVE(java_lang_Class_isPrimitive)
KAFFE_NATIVE(java_lang_Class_getName)
KAFFE_NATIVE(java_lang_Class_getClassLoader)  
KAFFE_NATIVE(java_lang_Class_getSuperclass)
KAFFE_NATIVE(java_lang_Class_getInterfaces)
KAFFE_NATIVE(java_lang_Class_getComponentType)
/* KAFFE_NATIVE(java_lang_Class_getModifiers) */
/* KAFFE_NATIVE(java_lang_Class_getSigners) */
/* KAFFE_NATIVE(java_lang_Class_setSigners) */
KAFFE_NATIVE(java_lang_Class_getPrimitiveClass)
KAFFE_NATIVE(java_lang_Class_getFields0)
/* KAFFE_NATIVE(java_lang_Class_getMethods0) */
/* KAFFE_NATIVE(java_lang_Class_getConstructors0) */
/* KAFFE_NATIVE(java_lang_Class_getField0) */
/* KAFFE_NATIVE(java_lang_Class_getMethod0) */
/* KAFFE_NATIVE(java_lang_Class_getConstructor0) */
KAFFE_NATIVE(java_lang_Compiler_initialize)
KAFFE_NATIVE(java_lang_Compiler_compileClass)
KAFFE_NATIVE(java_lang_Compiler_compileClasses)
KAFFE_NATIVE(java_lang_Compiler_command)
KAFFE_NATIVE(java_lang_Compiler_enable)
KAFFE_NATIVE(java_lang_Compiler_disable)
KAFFE_NATIVE(java_lang_Double_doubleToLongBits)
KAFFE_NATIVE(java_lang_Double_longBitsToDouble)
KAFFE_NATIVE(java_lang_Double_valueOf0)
KAFFE_NATIVE(java_lang_Float_floatToIntBits)
KAFFE_NATIVE(java_lang_Float_intBitsToFloat)
KAFFE_NATIVE(java_lang_Math_sin)
KAFFE_NATIVE(java_lang_Math_cos)
KAFFE_NATIVE(java_lang_Math_tan)
KAFFE_NATIVE(java_lang_Math_asin)
KAFFE_NATIVE(java_lang_Math_acos)
KAFFE_NATIVE(java_lang_Math_atan)
KAFFE_NATIVE(java_lang_Math_exp)
KAFFE_NATIVE(java_lang_Math_log)
KAFFE_NATIVE(java_lang_Math_sqrt)
KAFFE_NATIVE(java_lang_Math_IEEEremainder)
KAFFE_NATIVE(java_lang_Math_ceil)
KAFFE_NATIVE(java_lang_Math_floor)
KAFFE_NATIVE(java_lang_Math_rint)
KAFFE_NATIVE(java_lang_Math_atan2)
KAFFE_NATIVE(java_lang_Math_pow)
KAFFE_NATIVE(java_lang_Object_getClass)
KAFFE_NATIVE(java_lang_Object_hashCode)
KAFFE_NATIVE(java_lang_Object_clone)
KAFFE_NATIVE(java_lang_Object_notify)
KAFFE_NATIVE(java_lang_Object_notifyAll)  
KAFFE_NATIVE(java_lang_Object_wait)
/* KAFFE_NATIVE(java_lang_ProcessReaper_waitForDeath) */
KAFFE_NATIVE(java_lang_Runtime_exitInternal)
KAFFE_NATIVE(java_lang_Runtime_runFinalizersOnExit0)
KAFFE_NATIVE(java_lang_Runtime_execInternal)
KAFFE_NATIVE(java_lang_Runtime_freeMemory)
KAFFE_NATIVE(java_lang_Runtime_totalMemory)
KAFFE_NATIVE(java_lang_Runtime_gc)
KAFFE_NATIVE(java_lang_Runtime_runFinalization)
KAFFE_NATIVE(java_lang_Runtime_traceInstructions)
KAFFE_NATIVE(java_lang_Runtime_traceMethodCalls)
KAFFE_NATIVE(java_lang_Runtime_initializeLinkerInternal)
KAFFE_NATIVE(java_lang_Runtime_buildLibName)
KAFFE_NATIVE(java_lang_Runtime_loadFileInternal)
KAFFE_NATIVE(java_lang_SecurityManager_getClassContext)
KAFFE_NATIVE(java_lang_SecurityManager_currentClassLoader)
KAFFE_NATIVE(java_lang_SecurityManager_classDepth)
KAFFE_NATIVE(java_lang_SecurityManager_classLoaderDepth)
/* KAFFE_NATIVE(java_lang_SecurityManager_currentLoadedClass0) */
KAFFE_NATIVE(java_lang_String_intern)
KAFFE_NATIVE(java_lang_System_setIn0)
KAFFE_NATIVE(java_lang_System_setOut0)
KAFFE_NATIVE(java_lang_System_setErr0)
KAFFE_NATIVE(java_lang_System_currentTimeMillis)
KAFFE_NATIVE(java_lang_System_arraycopy)
KAFFE_NATIVE(java_lang_System_identityHashCode)
KAFFE_NATIVE(java_lang_System_initProperties)
KAFFE_NATIVE(java_lang_Thread_currentThread)
KAFFE_NATIVE(java_lang_Thread_yield)
KAFFE_NATIVE(java_lang_Thread_sleep)
KAFFE_NATIVE(java_lang_Thread_start)
/* KAFFE_NATIVE(java_lang_Thread_isInterrupted) */
KAFFE_NATIVE(java_lang_Thread_isAlive)
KAFFE_NATIVE(java_lang_Thread_countStackFrames)
KAFFE_NATIVE(java_lang_Thread_setPriority0)
KAFFE_NATIVE(java_lang_Thread_stop0)
KAFFE_NATIVE(java_lang_Thread_suspend0)
KAFFE_NATIVE(java_lang_Thread_resume0)
/* KAFFE_NATIVE(java_lang_Thread_interrupt0) */
KAFFE_NATIVE(java_lang_Throwable_printStackTrace0)
KAFFE_NATIVE(java_lang_Throwable_fillInStackTrace)
/* KAFFE_NATIVE(java_lang_UNIXProcess_run) */
/* KAFFE_NATIVE(java_lang_UNIXProcess_forkAndExec) */
/* KAFFE_NATIVE(java_lang_UNIXProcess_notifyReaders) */
KAFFE_NATIVE(java_lang_UNIXProcess_destroy) 

KAFFE_NATIVE(java_lang_reflect_Array_getLength)
KAFFE_NATIVE(java_lang_reflect_Array_get)
KAFFE_NATIVE(java_lang_reflect_Array_getBoolean)
KAFFE_NATIVE(java_lang_reflect_Array_getByte)
KAFFE_NATIVE(java_lang_reflect_Array_getChar)
KAFFE_NATIVE(java_lang_reflect_Array_getShort)
KAFFE_NATIVE(java_lang_reflect_Array_getInt)
KAFFE_NATIVE(java_lang_reflect_Array_getLong)
KAFFE_NATIVE(java_lang_reflect_Array_getFloat)
KAFFE_NATIVE(java_lang_reflect_Array_getDouble)
KAFFE_NATIVE(java_lang_reflect_Array_set)
KAFFE_NATIVE(java_lang_reflect_Array_setBoolean)
KAFFE_NATIVE(java_lang_reflect_Array_setByte)
KAFFE_NATIVE(java_lang_reflect_Array_setChar)
KAFFE_NATIVE(java_lang_reflect_Array_setShort)
KAFFE_NATIVE(java_lang_reflect_Array_setInt)
KAFFE_NATIVE(java_lang_reflect_Array_setLong)
KAFFE_NATIVE(java_lang_reflect_Array_setFloat)
KAFFE_NATIVE(java_lang_reflect_Array_setDouble)
KAFFE_NATIVE(java_lang_reflect_Array_newArray)
KAFFE_NATIVE(java_lang_reflect_Array_multiNewArray)
KAFFE_NATIVE(java_lang_reflect_Constructor_getModifiers)
KAFFE_NATIVE(java_lang_reflect_Constructor_newInstance)
/* KAFFE_NATIVE(java_lang_reflect_Field_getModifiers) */
/* KAFFE_NATIVE(java_lang_reflect_Field_get) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getBoolean) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getByte) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getChar) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getShort) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getInt) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getLong) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getFloat) */
/* KAFFE_NATIVE(java_lang_reflect_Field_getDouble) */
/* KAFFE_NATIVE(java_lang_reflect_Field_set) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setBoolean) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setByte) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setChar) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setShort) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setInt) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setLong) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setFloat) */
/* KAFFE_NATIVE(java_lang_reflect_Field_setDouble) */
/* KAFFE_NATIVE(java_lang_reflect_Method_getModifiers) */
KAFFE_NATIVE(java_lang_reflect_Method_invoke)

KAFFE_NATIVE(java_io_FileDescriptor_valid)
KAFFE_NATIVE(java_io_FileDescriptor_sync)
KAFFE_NATIVE(java_io_FileDescriptor_initSystemFD)
KAFFE_NATIVE(java_io_FileInputStream_open)
KAFFE_NATIVE(java_io_FileInputStream_read)
KAFFE_NATIVE(java_io_FileInputStream_readBytes)
KAFFE_NATIVE(java_io_FileInputStream_skip)
KAFFE_NATIVE(java_io_FileInputStream_available)
KAFFE_NATIVE(java_io_FileInputStream_close)
KAFFE_NATIVE(java_io_FileOutputStream_open)
KAFFE_NATIVE(java_io_FileOutputStream_openAppend)
KAFFE_NATIVE(java_io_FileOutputStream_write)
KAFFE_NATIVE(java_io_FileOutputStream_writeBytes)
KAFFE_NATIVE(java_io_FileOutputStream_close)
KAFFE_NATIVE(java_io_File_exists0)
KAFFE_NATIVE(java_io_File_canWrite0)
KAFFE_NATIVE(java_io_File_canRead0)
KAFFE_NATIVE(java_io_File_isFile0)
KAFFE_NATIVE(java_io_File_isDirectory0)
KAFFE_NATIVE(java_io_File_lastModified0)
KAFFE_NATIVE(java_io_File_length0)
KAFFE_NATIVE(java_io_File_mkdir0)
KAFFE_NATIVE(java_io_File_renameTo0)
KAFFE_NATIVE(java_io_File_delete0)
KAFFE_NATIVE(java_io_File_rmdir0)
KAFFE_NATIVE(java_io_File_list0)
KAFFE_NATIVE(java_io_File_canonPath)
KAFFE_NATIVE(java_io_File_isAbsolute)
/* KAFFE_NATIVE(java_io_ObjectInputStream_loadClass0) */
/* KAFFE_NATIVE(java_io_ObjectInputStream_inputClassFields) */
/* KAFFE_NATIVE(java_io_ObjectInputStream_inputArrayValues) */
/* KAFFE_NATIVE(java_io_ObjectInputStream_allocateNewObject) */
/* KAFFE_NATIVE(java_io_ObjectInputStream_allocateNewArray) */
/* KAFFE_NATIVE(java_io_ObjectInputStream_invokeObjectReader) */
/* KAFFE_NATIVE(java_io_ObjectOutputStream_outputClassFields) */
/* KAFFE_NATIVE(java_io_ObjectOutputStream_outputArrayValues) */
/* KAFFE_NATIVE(java_io_ObjectOutputStream_invokeObjectWriter) */
/* KAFFE_NATIVE(java_io_ObjectOutputStream_getRefHashCode) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getClassAccess) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getMethodSignatures) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getMethodAccess) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getFieldSignatures) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getFieldAccess) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getFields0) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_getSerialVersionUID) */
/* KAFFE_NATIVE(java_io_ObjectStreamClass_hasWriteObject) */
KAFFE_NATIVE(java_io_RandomAccessFile_open)
KAFFE_NATIVE(java_io_RandomAccessFile_read)
KAFFE_NATIVE(java_io_RandomAccessFile_readBytes)
KAFFE_NATIVE(java_io_RandomAccessFile_write)
KAFFE_NATIVE(java_io_RandomAccessFile_writeBytes)
KAFFE_NATIVE(java_io_RandomAccessFile_getFilePointer)
KAFFE_NATIVE(java_io_RandomAccessFile_seek) 
KAFFE_NATIVE(java_io_RandomAccessFile_length)
KAFFE_NATIVE(java_io_RandomAccessFile_close)

KAFFE_NATIVE(java_util_ResourceBundle_getClassContext)

#if defined(HAVE_LIBZ) && defined(HAVE_ZLIB_H)
KAFFE_NATIVE(java_util_zip_Adler32_update)
KAFFE_NATIVE(java_util_zip_Adler32_update1)
KAFFE_NATIVE(java_util_zip_CRC32_update)
KAFFE_NATIVE(java_util_zip_CRC32_update1)
KAFFE_NATIVE(java_util_zip_Deflater_setDictionary)
KAFFE_NATIVE(java_util_zip_Deflater_deflate)
KAFFE_NATIVE(java_util_zip_Deflater_getAdler)
KAFFE_NATIVE(java_util_zip_Deflater_getTotalIn)
KAFFE_NATIVE(java_util_zip_Deflater_getTotalOut)
KAFFE_NATIVE(java_util_zip_Deflater_reset)
KAFFE_NATIVE(java_util_zip_Deflater_end)
KAFFE_NATIVE(java_util_zip_Inflater_setDictionary)
KAFFE_NATIVE(java_util_zip_Inflater_inflate)
KAFFE_NATIVE(java_util_zip_Inflater_getAdler)
KAFFE_NATIVE(java_util_zip_Inflater_getTotalIn)
KAFFE_NATIVE(java_util_zip_Inflater_getTotalOut)
KAFFE_NATIVE(java_util_zip_Inflater_reset)
KAFFE_NATIVE(java_util_zip_Inflater_end)
KAFFE_NATIVE(java_util_zip_Inflater_init)
#endif /* defined(HAVE_LIBZ) && defined(HAVE_ZLIB_H) */

/* tjwassoc.co.uk/APInet */

KAFFE_NATIVE(java_net_InetAddressImpl_getLocalHostName)
KAFFE_NATIVE(java_net_InetAddressImpl_makeAnyLocalAddress)
KAFFE_NATIVE(java_net_InetAddressImpl_lookupAllHostAddr)
KAFFE_NATIVE(java_net_InetAddressImpl_getHostByAddr)
KAFFE_NATIVE(java_net_InetAddressImpl_getInetFamily)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_datagramSocketCreate)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_bind)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_send)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_peek)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_receive)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_datagramSocketClose)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_setTTL)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_getTTL)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_join)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_leave)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_socketSetOption)
KAFFE_NATIVE(java_net_PlainDatagramSocketImpl_socketGetOption)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketCreate)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketConnect)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketBind)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketListen)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketAccept)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketAvailable)
KAFFE_NATIVE(java_net_PlainSocketImpl_socketClose)
KAFFE_NATIVE(java_net_PlainSocketImpl_initProto)
/* KAFFE_NATIVE(java_net_PlainSocketImpl_socketSetOption) */
/* KAFFE_NATIVE(java_net_PlainSocketImpl_socketGetOption) */
KAFFE_NATIVE(java_net_SocketInputStream_socketRead)
KAFFE_NATIVE(java_net_SocketOutputStream_socketWrite)
