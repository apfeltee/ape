        //TODO: string parsing is kind of broken! in particular, it will not parse "\\", or "\""
        const char* scanString(const char delimiter, bool is_template, bool* out_template_found, int* out_len)
        {
            enum{ kTempSize = 512 };
            int len;
            int last;
            int startpos;
            bool escaped;
            //char tmpsrc[kTempSize + 1] = {0};
            *out_len = 0;
            escaped = false;
            startpos = m_position;
            int prevchar = m_currchar;

            /*if(peekChar(-1) == delimiter)
            {
                readChar();
                goto endscan;
            }*/
            #if 0
            while(true)
            {
                auto peek0 = peekChar(0);
                auto peek1 = peekChar(1);
                auto peekprev = peekChar(-1);
                fprintf(stderr, "in scanString(startpos=%d, m_position=%d) m_currchar=(%d)%c, prevchar=(%d)%c peekchar(-1)=(%d)%c peekChar(0)=(%d)%c peekChar(1)=(%d)%c ...\n", startpos, m_position, m_currchar, m_currchar, prevchar, prevchar, peekprev, peekprev, peek0, peek0, peek1, peek1);


                if(m_currchar == '\0')
                {
                    m_errors->addFormat(APE_ERROR_PARSING, m_currtoken.pos, "unexpected EOF while parsing string");
                    return NULL;
                }
                if(m_currchar == delimiter)
                {
                    if(!escaped)
                    {
                        fprintf(stderr, "in scanString: startpos=%d m_position=%d\n", startpos, m_position);
                        if(startpos == m_position)
                        {
                            startpos++;
                        }
                        break;
                    }
                }
                if(is_template && !escaped && (m_currchar == '$') && (this->peekChar() == '{'))
                {
                    *out_template_found = true;
                    break;
                }
                escaped = false;
                if((m_currchar == '\\'))
                {
                    /* this is completely ridiculous */
                    fprintf(stderr, "peekChar: 0=<%c>, 1=<%c>, 2=<%c>\n", peekChar(0), peekChar(1), peekChar(2));
                    if((peekChar(0) == '\\') && (peekChar(1) == delimiter))
                    {
                        //fprintf(stderr, "scanString: endscan!\n");
                        readChar();
                        readChar();
                        break;
                    }
                    if((peekChar() != '\\'))
                    {
                        escaped = true;
                    }
                }
                prevchar = m_currchar;
                this->readChar();
            }
            #else
            while(true)
            {
                auto peek0 = peekChar(0);
                auto peek1 = peekChar(1);
                auto peekprev = peekChar(-1);
                fprintf(stderr, "in scanString(startpos=%d, m_position=%d) m_currchar=(%d)%c, prevchar=(%d)%c peekchar(-1)=(%d)%c peekChar(0)=(%d)%c peekChar(1)=(%d)%c ...\n", startpos, m_position, m_currchar, m_currchar, prevchar, prevchar, peekprev, peekprev, peek0, peek0, peek1, peek1);

                if(m_currchar == '\0')
                {
                    m_errors->addFormat(APE_ERROR_PARSING, m_currtoken.pos, "unexpected EOF while parsing string");
                    return NULL;
                }
                if(m_currchar == delimiter)
                {
                    if(startpos == m_position)
                    {
                        //startpos++;
                        readChar();
                    }
                    break;
                }
                prevchar = m_currchar;
                readChar();
     
            }
            #endif
            endscan:
            /* this is also ridiculous. */
            last = m_sourcedata[startpos];
            //memset(tmpsrc, 0, kTempSize);
            //memcpy(tmpsrc, &m_sourcedata[startpos], m_position-startpos);
            //fprintf(stderr, "scanString(delimiter=(%d)%c, start=%d, source=<<<%s>>>): last=%c\n", delimiter, delimiter, startpos, tmpsrc, last);
            if(last == delimiter)
            {
                //fprintf(stderr, "scanString(): reading remainder...\n");
                this->readChar();
            }
            len = m_position - startpos;
            *out_len = len;
            return m_sourcedata + startpos;
        }
