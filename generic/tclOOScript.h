/*
 * tclOOScript.h --
 *
 *	This file contains support scripts for TclOO. They are defined here so
 *	that the code can be definitely run even in safe interpreters; TclOO's
 *	core setup is safe.
 *
 * Copyright (c) 2012-2018 Donal K. Fellows
 * Copyright (c) 2013 Andreas Kupries
 * Copyright (c) 2017 Gerald Lester
 *
 * See the file "license.terms" for information on usage and redistribution of
 * this file, and for a DISCLAIMER OF ALL WARRANTIES.
 */

#ifndef TCL_OO_SCRIPT_H
#define TCL_OO_SCRIPT_H

/*
 * The scripted part of the definitions of TclOO.
 *
 * Compiled from generic/tclOOScript.tcl by tools/makeHeader.tcl, which
 * contains the commented version of everything; *this* file is automatically
 * generated.
 */

static const char *tclOOSetupScript =
/* !BEGIN!: Do not edit below this line. */
"::namespace eval ::oo {\n"
"\t::namespace path {}\n"
"\tnamespace eval Helpers {\n"
"\t\t::namespace path {}\n"
"\t\tproc callback {method args} {\n"
"\t\t\tlist [uplevel 1 {::namespace which my}] $method {*}$args\n"
"\t\t}\n"
"\t\tnamespace export callback\n"
"\t\tnamespace eval tmp {namespace import ::oo::Helpers::callback}\n"
"\t\tnamespace export -clear\n"
"\t\trename tmp::callback mymethod\n"
"\t\tnamespace delete tmp\n"
"\t\tproc classvariable {name args} {\n"
"\t\t\tset ns [info object namespace [uplevel 1 {self class}]]\n"
"\t\t\tforeach v [list $name {*}$args] {\n"
"\t\t\t\tif {[string match *(*) $v]} {\n"
"\t\t\t\t\tset reason \"can\'t create a scalar variable that looks like an array element\"\n"
"\t\t\t\t\treturn -code error -errorcode {TCL UPVAR LOCAL_ELEMENT} \\\n"
"\t\t\t\t\t\t[format {bad variable name \"%s\": %s} $v $reason]\n"
"\t\t\t\t}\n"
"\t\t\t\tif {[string match *::* $v]} {\n"
"\t\t\t\t\tset reason \"can\'t create a local variable with a namespace separator in it\"\n"
"\t\t\t\t\treturn -code error -errorcode {TCL UPVAR INVERTED} \\\n"
"\t\t\t\t\t\t[format {bad variable name \"%s\": %s} $v $reason]\n"
"\t\t\t\t}\n"
"\t\t\t\tlappend vs $v $v\n"
"\t\t\t}\n"
"\t\t\ttailcall namespace upvar $ns {*}$vs\n"
"\t\t}\n"
"\t\tproc link {args} {\n"
"\t\t\tset ns [uplevel 1 {::namespace current}]\n"
"\t\t\tforeach link $args {\n"
"\t\t\t\tif {[llength $link] == 2} {\n"
"\t\t\t\t\tlassign $link src dst\n"
"\t\t\t\t} elseif {[llength $link] == 1} {\n"
"\t\t\t\t\tlassign $link src\n"
"\t\t\t\t\tset dst $src\n"
"\t\t\t\t} else {\n"
"\t\t\t\t\treturn -code error -errorcode {TCLOO CMDLINK FORMAT} \\\n"
"\t\t\t\t\t\t\"bad link description; must only have one or two elements\"\n"
"\t\t\t\t}\n"
"\t\t\t\tif {![string match ::* $src]} {\n"
"\t\t\t\t\tset src [string cat $ns :: $src]\n"
"\t\t\t\t}\n"
"\t\t\t\tinterp alias {} $src {} ${ns}::my $dst\n"
"\t\t\t\ttrace add command ${ns}::my delete [list \\\n"
"\t\t\t\t\t::oo::UnlinkLinkedCommand $src]\n"
"\t\t\t}\n"
"\t\t\treturn\n"
"\t\t}\n"
"\t}\n"
"\tproc UnlinkLinkedCommand {cmd args} {\n"
"\t\tif {[namespace which $cmd] ne {}} {\n"
"\t\t\trename $cmd {}\n"
"\t\t}\n"
"\t}\n"
"\tproc DelegateName {class} {\n"
"\t\tstring cat [info object namespace $class] {:: oo ::delegate}\n"
"\t}\n"
"\tproc MixinClassDelegates {class} {\n"
"\t\tif {![info object isa class $class]} {\n"
"\t\t\treturn\n"
"\t\t}\n"
"\t\tset delegate [DelegateName $class]\n"
"\t\tif {![info object isa class $delegate]} {\n"
"\t\t\treturn\n"
"\t\t}\n"
"\t\tforeach c [info class superclass $class] {\n"
"\t\t\tset d [DelegateName $c]\n"
"\t\t\tif {![info object isa class $d]} {\n"
"\t\t\t\tcontinue\n"
"\t\t\t}\n"
"\t\t\tdefine $delegate ::oo::define::superclass -append $d\n"
"\t\t}\n"
"\t\tobjdefine $class ::oo::objdefine::mixin -append $delegate\n"
"\t}\n"
"\tproc UpdateClassDelegatesAfterClone {originObject targetObject} {\n"
"\t\tset originDelegate [DelegateName $originObject]\n"
"\t\tset targetDelegate [DelegateName $targetObject]\n"
"\t\tif {\n"
"\t\t\t[info object isa class $originDelegate]\n"
"\t\t\t&& ![info object isa class $targetDelegate]\n"
"\t\t} then {\n"
"\t\t\tcopy $originDelegate $targetDelegate\n"
"\t\t\tobjdefine $targetObject ::oo::objdefine::mixin -set \\\n"
"\t\t\t\t{*}[lmap c [info object mixin $targetObject] {\n"
"\t\t\t\t\tif {$c eq $originDelegate} {set targetDelegate} {set c}\n"
"\t\t\t\t}]\n"
"\t\t}\n"
"\t}\n"
"\tproc define::classmethod {name {argv {}} {body {}}} {\n"
"\t\t::set argc [::llength [::info level 0]]\n"
"\t\t::if {$argc == 3} {\n"
"\t\t\t::return -code error -errorcode {TCL WRONGARGS} [::format \\\n"
"\t\t\t\t{wrong # args: should be \"%s name \?args body\?\"} \\\n"
"\t\t\t\t[::lindex [::info level 0] 0]]\n"
"\t\t}\n"
"\t\t::set cls [::uplevel 1 self]\n"
"\t\t::if {$argc == 4} {\n"
"\t\t\t::oo::define [::oo::DelegateName $cls] method $name $argv $body\n"
"\t\t}\n"
"\t\t::tailcall forward $name myclass $name\n"
"\t}\n"
"\tproc define::initialise {body} {\n"
"\t\t::set clsns [::info object namespace [::uplevel 1 self]]\n"
"\t\t::tailcall apply [::list {} $body $clsns]\n"
"\t}\n"
"\tnamespace eval define {\n"
"\t\t::namespace export initialise\n"
"\t\t::namespace eval tmp {::namespace import ::oo::define::initialise}\n"
"\t\t::namespace export -clear\n"
"\t\t::rename tmp::initialise initialize\n"
"\t\t::namespace delete tmp\n"
"\t}\n"
"\tdefine Slot {\n"
"\t\tmethod Get {} {\n"
"\t\t\treturn -code error -errorcode {TCLOO ABSTRACT_SLOT} \"unimplemented\"\n"
"\t\t}\n"
"\t\tmethod Set list {\n"
"\t\t\treturn -code error -errorcode {TCLOO ABSTRACT_SLOT} \"unimplemented\"\n"
"\t\t}\n"
"\t\tmethod Resolve list {\n"
"\t\t\treturn $list\n"
"\t\t}\n"
"\t\tmethod -set args {\n"
"\t\t\tset my [namespace which my]\n"
"\t\t\tset args [lmap a $args {uplevel 1 [list $my Resolve $a]}]\n"
"\t\t\ttailcall my Set $args\n"
"\t\t}\n"
"\t\tmethod -append args {\n"
"\t\t\tset my [namespace which my]\n"
"\t\t\tset args [lmap a $args {uplevel 1 [list $my Resolve $a]}]\n"
"\t\t\tset current [uplevel 1 [list $my Get]]\n"
"\t\t\ttailcall my Set [list {*}$current {*}$args]\n"
"\t\t}\n"
"\t\tmethod -clear {} {tailcall my Set {}}\n"
"\t\tmethod -prepend args {\n"
"\t\t\tset my [namespace which my]\n"
"\t\t\tset args [lmap a $args {uplevel 1 [list $my Resolve $a]}]\n"
"\t\t\tset current [uplevel 1 [list $my Get]]\n"
"\t\t\ttailcall my Set [list {*}$args {*}$current]\n"
"\t\t}\n"
"\t\tmethod -remove args {\n"
"\t\t\tset my [namespace which my]\n"
"\t\t\tset args [lmap a $args {uplevel 1 [list $my Resolve $a]}]\n"
"\t\t\tset current [uplevel 1 [list $my Get]]\n"
"\t\t\ttailcall my Set [lmap val $current {\n"
"\t\t\t\tif {$val in $args} continue else {set val}\n"
"\t\t\t}]\n"
"\t\t}\n"
"\t\tforward --default-operation my -append\n"
"\t\tmethod unknown {args} {\n"
"\t\t\tset def --default-operation\n"
"\t\t\tif {[llength $args] == 0} {\n"
"\t\t\t\ttailcall my $def\n"
"\t\t\t} elseif {![string match -* [lindex $args 0]]} {\n"
"\t\t\t\ttailcall my $def {*}$args\n"
"\t\t\t}\n"
"\t\t\tnext {*}$args\n"
"\t\t}\n"
"\t\texport -set -append -clear -prepend -remove\n"
"\t\tunexport unknown destroy\n"
"\t}\n"
"\tobjdefine define::superclass forward --default-operation my -set\n"
"\tobjdefine define::mixin forward --default-operation my -set\n"
"\tobjdefine objdefine::mixin forward --default-operation my -set\n"
"\tdefine object method <cloned> {originObject} {\n"
"\t\tforeach p [info procs [info object namespace $originObject]::*] {\n"
"\t\t\tset args [info args $p]\n"
"\t\t\tset idx -1\n"
"\t\t\tforeach a $args {\n"
"\t\t\t\tif {[info default $p $a d]} {\n"
"\t\t\t\t\tlset args [incr idx] [list $a $d]\n"
"\t\t\t\t} else {\n"
"\t\t\t\t\tlset args [incr idx] [list $a]\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t\tset b [info body $p]\n"
"\t\t\tset p [namespace tail $p]\n"
"\t\t\tproc $p $args $b\n"
"\t\t}\n"
"\t\tforeach v [info vars [info object namespace $originObject]::*] {\n"
"\t\t\tupvar 0 $v vOrigin\n"
"\t\t\tnamespace upvar [namespace current] [namespace tail $v] vNew\n"
"\t\t\tif {[info exists vOrigin]} {\n"
"\t\t\t\tif {[array exists vOrigin]} {\n"
"\t\t\t\t\tarray set vNew [array get vOrigin]\n"
"\t\t\t\t} else {\n"
"\t\t\t\t\tset vNew $vOrigin\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t}\n"
"\t}\n"
"\tdefine class method <cloned> {originObject} {\n"
"\t\tnext $originObject\n"
"\t\t::oo::UpdateClassDelegatesAfterClone $originObject [self]\n"
"\t}\n"
"\tclass create singleton {\n"
"\t\tsuperclass class\n"
"\t\tvariable object\n"
"\t\tunexport create createWithNamespace\n"
"\t\tmethod new args {\n"
"\t\t\tif {![info exists object] || ![info object isa object $object]} {\n"
"\t\t\t\tset object [next {*}$args]\n"
"\t\t\t\t::oo::objdefine $object {\n"
"\t\t\t\t\tmethod destroy {} {\n"
"\t\t\t\t\t\t::return -code error -errorcode {TCLOO SINGLETON} \\\n"
"\t\t\t\t\t\t\t\"may not destroy a singleton object\"\n"
"\t\t\t\t\t}\n"
"\t\t\t\t\tmethod <cloned> {originObject} {\n"
"\t\t\t\t\t\t::return -code error -errorcode {TCLOO SINGLETON} \\\n"
"\t\t\t\t\t\t\t\"may not clone a singleton object\"\n"
"\t\t\t\t\t}\n"
"\t\t\t\t}\n"
"\t\t\t}\n"
"\t\t\treturn $object\n"
"\t\t}\n"
"\t}\n"
"\tclass create abstract {\n"
"\t\tsuperclass class\n"
"\t\tunexport create createWithNamespace new\n"
"\t}\n"
"}\n"
/* !END!: Do not edit above this line. */
;

#endif /* TCL_OO_SCRIPT_H */

/*
 * Local Variables:
 * mode: c
 * c-basic-offset: 4
 * fill-column: 78
 * End:
 */
