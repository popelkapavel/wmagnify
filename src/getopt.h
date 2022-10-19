// getopt
// mingw32

int optind=1,optptr=0;
char optopt,*optarg=0;

int getopt(int argc, char * const * argv,const char *options) {
  char *ap,*cp;
  optarg=0;
 arg:
  if(optind>=argc)
    return -1;
  ap=argv[optind]+optptr;
  if(!*ap) {
    optind++,optptr=0;
    goto arg;
  }
  if(!optptr) {
    if(*ap=='-') {
      if(ap[1]=='-'&&!ap[2]) {
        optind++;
        optptr=0;
        return -1;
      }
      optptr++;
      ap++;
    } else
      return -1;
  }
  optptr++;
  optopt=*ap++;
  cp=strchr(options,optopt);
  if(!cp)
    return '?';
  if(cp[1]==':') {
    if(*ap) {
      optarg=ap;
    } else
      optind++,optarg=argv[optind];
    optptr=strlen(argv[optind]);
  }
  return optopt;
}

