;
; // Launch Service Message Texts
;


;
; // Header
;

SeverityNames=(Success=0x0:STATUS_SEVERITY_SUCCESS
               Informational=0x1:STATUS_SEVERITY_INFORMATIONAL
               Warning=0x2:STATUS_SEVERITY_WARNING
               Error=0x3:STATUS_SEVERITY_ERROR
              )


FacilityNames=(System=0x0:FACILITY_SYSTEM
               Runtime=0x2:FACILITY_RUNTIME
               Stubs=0x3:FACILITY_STUBS
               Io=0x4:FACILITY_IO_ERROR_CODE
              )

LanguageNames=(English=0x409:MSG00409)


;
; // Event Categories
;

MessageIdTypedef=WORD

MessageId=0x1
SymbolicName=SVC_CATEGORY
Language=English
MsMpi Launch Service Events
.

MessageId=0x2
SymbolicName=CLIENT_CATEGORY
Language=English
MsMpi Launch Service Client Events
.


;
; // Message Definitions
;

MessageIdTypedef=DWORD

MessageId=0x1
Severity=Success
Facility=Runtime
SymbolicName=SERVICE_EVENT
Language=English
%1
.

MessageId=0x100
Severity=Success
Facility=Runtime
SymbolicName=SERVICE_STARTED
Language=English
MsMpi Launch Service started succesfully.
.


MessageId=0x101
Severity=Informational
Facility=Runtime
SymbolicName=SERVICE_STATE_CHANGE
Language=English
MsMpi Launch Service state change from %1 to %2.
.


MessageId=0x102
Severity=Error
Facility=System
;// !!!!!!!!!!!!!!!
SymbolicName=MSG_BAD_FILE_CONTENTS
Language=English
File %1 contains content that is not valid.
.
